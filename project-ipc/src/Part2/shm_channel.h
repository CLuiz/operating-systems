#ifndef _SHM_CHANNEL_H_
#define _SHM_CHANNEL_H_

// shared memory defaults
#define MAX_CACHE_REQUEST_LEN 256

// message queue defaults
#define MQ_NAME "/message_queue"
#define MAX_MSG_SIZE 4096
#define MAX_MSG_NUM 10

/**
 * Structure for shared memory writen between simplecache and webproxy
 */
typedef struct shm_segment {
    // semaphores used to synchronize read/writes
    sem_t cache_sem;
    sem_t proxy_sem;

    // transfer information
    ssize_t bytes_sent;
    
    // file information
    ssize_t file_size;
    char buffer[];
} shm_seg;

/**
 * Logs error that occured and exits with failure
 */
void error_and_die(const char *msg);

#endif
