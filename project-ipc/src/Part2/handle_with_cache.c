#include <stdlib.h>
#include <curl/curl.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <mqueue.h>

#include "gfserver.h"
#include "shm_channel.h"

int num_seqments;
size_t region_size;
steque_t shm_queue;

mqd_t msg_queue;

pthread_mutex_t shm_queue_mtx;
pthread_cond_t shm_queue_cond;

/**
 * Method for cleaning up all cache constructions
 */
void cache_cleanup() {
    char shm_name[MAX_CACHE_REQUEST_LEN] = {0};

    // destroy share memory queue
    steque_destroy(&shm_queue);

    // destroy all mutex constructs
    pthread_mutex_destroy(&shm_queue_mtx);
    pthread_cond_destroy(&shm_queue_cond);

    // free up all shared memory
    for (long i = 0; i < num_seqments; i++) {
        sprintf(shm_name, "/proxy_cache_%ld", i);
        shm_unlink(shm_name);
    }

    // clean up message queue
    mq_unlink(MQ_NAME);
    close(msg_queue);
}

/**
 * Method for initializing the shared memory cache and all constructs
 * that it will require.
 */
void cache_init(int nsegments, int size_seg) {
    cache_cleanup();

    num_seqments = nsegments;
    int shm_fd;

    // build share memory name and size
    char shm_name[MAX_CACHE_REQUEST_LEN];
    region_size = sizeof(shm_seg) + size_seg;

    // initialize steque
    steque_init(&shm_queue);

    // create shared memory to be used
    for (long i = 0; i < nsegments; i++) {
        // create shared memory name with correct id number
        sprintf(shm_name, "/proxy_cache_%ld", i);

        // unlink in case left over then open share memory file descriptor
        if ((shm_fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0666)) < 0)
            error_and_die("Cache init failed, shm_open");

        // allocate memory necessary for share memory file
        if ((ftruncate(shm_fd, (off_t) region_size)) < 0)
            error_and_die("Cache init failed, ftruncate");

        // map shared memory
        void *shm_pointer = mmap(NULL, region_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (shm_pointer == MAP_FAILED)
            error_and_die("Cache init failed, mmap");

        // push newly created shared memory to queue and close file descriptor
        steque_push(&shm_queue, (steque_item) i);
        close(shm_fd);
    }

    pthread_mutex_init(&shm_queue_mtx, NULL);
    pthread_cond_init(&shm_queue_cond, NULL);

    // set message queue attributes
    struct mq_attr msg_queue_attr;
    msg_queue_attr.mq_flags = 0;
    msg_queue_attr.mq_maxmsg = MAX_MSG_NUM;
    msg_queue_attr.mq_msgsize = MAX_MSG_SIZE;
    msg_queue_attr.mq_curmsgs = 0;

    // clean up and attempt to open message queue
    msg_queue = mq_open(MQ_NAME, O_CREAT | O_WRONLY, 0644, &msg_queue_attr);
    if (msg_queue < 0)
        error_and_die("Cache init failed, message queue open");
}

/**
 * Replaces share memory segment on queue when finished using
 */
void enqueue_segment(long shm_id) {
    // lock mutex and add back segment
    pthread_mutex_lock(&shm_queue_mtx);
    steque_enqueue(&shm_queue, (steque_item) shm_id);

    // unlock mutex and broadcast queue is ready
    pthread_mutex_unlock(&shm_queue_mtx);
    pthread_cond_broadcast(&shm_queue_cond);
}

/**
 * Handler method for web proxing for delivering data via a shared
 * memory cache.
 */
ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg) {
    size_t bytes_transferred = 0;
    ssize_t bytes_written;
    char buffer[region_size];

    // lock and check queue, signal waiting
    pthread_mutex_lock(&shm_queue_mtx);
    while(steque_isempty(&shm_queue))
        pthread_cond_wait(&shm_queue_cond, &shm_queue_mtx);

    // pop next segment from queue then unlock mutex
    long shm_id = (long) steque_pop(&shm_queue);
    pthread_mutex_unlock(&shm_queue_mtx);

    // extract shared memory name
    char shm_name[MAX_CACHE_REQUEST_LEN] = {0};
    if (sprintf(shm_name, "proxy_cache_%ld", shm_id) < 0)
        error_and_die("Handler failed to extract shm ID");

    // open shared memory
    int shm_fd = shm_open(shm_name, O_RDWR, 0);
    if (shm_fd == -1)
        error_and_die("Handler failed to open shm");

    // access shared memory when received and close file descriptor
    shm_seg *shm_pointer;
    shm_pointer = mmap(NULL, region_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_pointer == MAP_FAILED)
        error_and_die("Hanlder failed, mmap");
    close(shm_fd);

    // initialize share memory semaphores for synchronization, instead of message queues :)
    if ((sem_init(&shm_pointer->cache_sem, 1, 0) == -1) ||
            (sem_init(&shm_pointer->proxy_sem, 1, 0) == -1))
        error_and_die("Handler semaphores not initialized");

    // message simplecached
    char message[MAX_MSG_SIZE] = {0};
    sprintf(message, "Request: %ld %zu %s", shm_id, region_size, path);
    if (mq_send(msg_queue, message, strlen(message), 0) < 0)
        error_and_die("Handler failed to send message");

    // wait for semaphore to unlock from simplecache
    if (sem_wait(&shm_pointer->proxy_sem) == -1)
        error_and_die("Handler unable to lock semaphore");

    // if no file then return not found
    if (shm_pointer->file_size <= 0) {
        enqueue_segment(shm_id);
        // EXTRA CREDIT - HANDLE WITH CURL
        return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
    }

    // send ok header and start file transfer process
    gfs_sendheader(ctx, GF_OK, (size_t) shm_pointer->file_size);
    while (bytes_transferred < shm_pointer->file_size){

        // inform simplecache to write more
        if (sem_post(&shm_pointer->cache_sem) == -1)
            error_and_die("Handler failed to request data");
            
        // wait for cache to write data
        if (sem_wait(&shm_pointer->proxy_sem) == -1)
            error_and_die("Handler failed to recieve data");

        // set memory for local buffer and copy over shared buffer
        memset(buffer, 0, sizeof(buffer));
        memcpy(buffer, &shm_pointer->buffer, shm_pointer->bytes_sent);

        // attempt to write the file and check to ensure it was successful
        bytes_written = gfs_send(ctx, buffer, (size_t) shm_pointer->bytes_sent);
        if (bytes_written != shm_pointer->bytes_sent)
            error_and_die("Handler write error");

        // update the bytes transferred
        bytes_transferred += bytes_written;
    }

    // clean up handler and return
    enqueue_segment(shm_id);
    return bytes_transferred;
}
