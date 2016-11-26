#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <sys/fcntl.h>
#include <semaphore.h>
#include <mqueue.h>
#include <sys/mman.h>
#include <string.h>

#include "shm_channel.h"
#include "simplecache.h"

mqd_t msg_queue;

// new methods used in simplecached
void start_threads(int nthreads);
void *handler_worker(void* arg);
void simplecache_cleanup();

static void _sig_handler(int signo){
    if (signo == SIGINT || signo == SIGTERM){
        /* Unlink IPC mechanisms here*/
        fprintf(stderr, "\n\nSimplecache stopping\n\n");

        simplecache_cleanup();
        exit(signo);
    }
}

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"      \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
        {"nthreads",           required_argument,      NULL,           't'},
        {"cachedir",           required_argument,      NULL,           'c'},
        {"help",               no_argument,            NULL,           'h'},
        {NULL,                 0,                      NULL,             0}
};

void Usage() {
    fprintf(stdout, "%s", USAGE);
}

int main(int argc, char **argv) {
    int nthreads = 1;
    char *cachedir = "locals.txt";
    char option_char;

    while ((option_char = (char) getopt_long(argc, argv, "t:c:h", gLongOptions, NULL)) != -1) {
        switch (option_char) {
            case 't': // thread-count
                nthreads = atoi(optarg);
                break;
            case 'c': //cache directory
                cachedir = optarg;
                break;
            case 'h': // help
                Usage();
                exit(0);
            default:
                Usage();
                exit(1);
        }
    }

    if (signal(SIGINT, _sig_handler) == SIG_ERR){
        fprintf(stderr,"Can't catch SIGINT...exiting.\n");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGTERM, _sig_handler) == SIG_ERR){
        fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
        exit(EXIT_FAILURE);
    }

    /* Initializing the cache */
    simplecache_init(cachedir);

    // open message queues
    while (1) {
        if ((msg_queue = mq_open(MQ_NAME, O_RDONLY)) != -1)
            break;
    }
    
    // initialize and start pthreads
    start_threads(nthreads);

    // clean up and exit
    simplecache_destroy();
    return EXIT_SUCCESS;
}

/**
 * Initiates pthreads and then starts them waiting for messages to come through
 * joining when they are completed
 */
void start_threads(int nthreads) {
    pthread_t *worker_threads = malloc(nthreads * sizeof(pthread_t));

    // create pthreads
    for(int i = 0; i < nthreads; i++) {
        if (pthread_create(&worker_threads[i], NULL, handler_worker, NULL) != 0)
            error_and_die("Simplecache unable to initate threads");
    }

    // wait until threads join
    for (int i = 0; i < nthreads; i++)
        pthread_join(worker_threads[i], NULL);
}

/**
 * Handler function for worker thread to process request, check if file
 * exists and then write the file to shared memory
 */
void *handler_worker(void* arg) {
    char message[MAX_MSG_SIZE] = {0};
    char shm_name[MAX_CACHE_REQUEST_LEN] = {0};
    long shm_id;
    size_t region_size;
    char path[MAX_CACHE_REQUEST_LEN];
    size_t bytes_transferred;
    ssize_t bytes_sent;
    size_t transfer_size;

    while (1) {
        // receive message from mqueue
        memset(message, 0, sizeof(message));
        if (mq_receive(msg_queue, message, sizeof(message), 0) < 0)
            error_and_die("Simplecache error receiving message");

        // extract file information from message
        sscanf(message, "Request: %ld %zu %s", &shm_id, &region_size, path);
        int file_desc = simplecache_get(path);

        // get share memory name from its stored id
        if (sprintf(shm_name, "proxy_cache_%ld", shm_id) < 0)
            error_and_die("translation error");

        // open shared memory file descriptor
        int shm_fd = shm_open(shm_name, O_RDWR, 0);
        if (shm_fd == -1)
            error_and_die("Simplecache unable to open fd");

        // obtain the pointer to shared memory
        shm_seg *shm_pointer;
        shm_pointer = mmap(NULL, region_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (shm_pointer == MAP_FAILED)
            error_and_die("not able to map memory");
        close(shm_fd);

        // access file and update file size
        ssize_t file_size = lseek(file_desc, 0, SEEK_END);
        lseek(file_desc, 0, SEEK_SET);
        shm_pointer->file_size = file_size;

        // Send message back on status of file
        if (sem_post(&shm_pointer->proxy_sem) == -1)
            error_and_die("Simplecache failed to inform webroxy");

        // file not found so move back to beginning
        if (file_desc == -1)
            continue;

        // transfer file to webproxy
        bytes_transferred = 0;
        char buffer[region_size];
        while (bytes_transferred < file_size) {

            // wait for handler to say its ready
            if (sem_wait(&shm_pointer->cache_sem) == -1)
                error_and_die("Simplecache failed to wait for proxy");

            // calculate size of transfer to read in
            transfer_size = file_size - bytes_transferred;
            transfer_size = (transfer_size < region_size) ? transfer_size : region_size;

            // set memory for local buffer and read file
            memset(buffer, 0, sizeof(buffer));
            bytes_sent = read(file_desc, buffer, transfer_size);

            // make sure file read correctly and update totals
            if (bytes_sent == -1 || bytes_sent != transfer_size)
                error_and_die("simplecache read error");

            // set memory for shared buffer
            memset(&shm_pointer->buffer, 0, bytes_sent);
            memcpy(&shm_pointer->buffer, buffer, bytes_sent);

            // update the total bytes read and how much is being sent
            shm_pointer->bytes_sent = bytes_sent;
            bytes_transferred += bytes_sent;
            
            // Send message back on status of file
            if (sem_post(&shm_pointer->proxy_sem) == -1)
                error_and_die("Simplecache failed to inform webroxy");
        }
    }

    // to rid of stupid warning
    return NULL;
}

/**
 * Handles cleanup for simplecache
 */
void simplecache_cleanup() {

    // unlink and close message queue
    mq_unlink(MQ_NAME);
    close(msg_queue);
}
