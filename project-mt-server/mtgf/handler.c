#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "gfserver.h"
#include "content.h"
#include "steque.h"

#define BUFFER_SIZE 4096

// global variables to be initialized and used
pthread_mutex_t queue_mutex;
steque_t *context_queue;

void *context_handler(int thread_num);

// thread context structure used in thread processing
typedef struct thread_context_t {
    gfcontext_t *ctx;
    char *path;
    void *arg;
	ssize_t bytes_transferred;
} thread_context_t;

/**
 * Initializes all global variables necessary
 * @param num_threads - number of threads to create
 */
void global_init(int num_threads) {
	// create queue for storing request contexts
	context_queue = malloc(sizeof(steque_t));
	steque_init(context_queue);

	// initiate mutex and pthreads
	pthread_mutex_init(&queue_mutex, NULL);
	pthread_t thread[num_threads];

	// create all threads for when requests come
	int i;
	for(i = 0; i < num_threads; i++) {
        if (pthread_create(&(thread[i]), NULL, (void *)context_handler, &(i)) < 0)
			fprintf(stderr, "Thread %d was unable to create.\n", i);
    }
}

/**
 * Handler function for incoming request.  Takes context of the request and
 * adds it to the queueu to be processed.
 * @param *ctx - pointer to request context
 * @param *path - path to file
 * @param *arg - handler argument
 */
ssize_t handler_get(gfcontext_t *ctx, char *path, void *arg) {
	// create the threads context
	thread_context_t *context = malloc(sizeof(thread_context_t));
	context->ctx = ctx;
	context->path = path;
	context->arg = arg;
	context->bytes_transferred = 0;

	// queue up thread and return nothing
	steque_enqueue(context_queue, context);
	return 0;
}

/**
 * Context handler for pthread when it comes off the queue and begins processing.
 * @param thread_num - number of the thread created
 */
void *context_handler(int thread_num) {
	int fildes;
	ssize_t file_len;
	ssize_t read_len, write_len;
	char buffer[BUFFER_SIZE];
	thread_context_t *context = NULL;

	while (1) {

        /*if nothing in queue or null retrieved continue checking*/
		if (steque_isempty(context_queue))
			continue;

		/*Check if anything in queue, if so take it off*/
		if (!steque_isempty(context_queue)) {
			pthread_mutex_lock(&queue_mutex);
	        context = (thread_context_t *) steque_pop(context_queue);
	        pthread_mutex_unlock(&queue_mutex);
            if (context == NULL)
                continue;
		}

		/*Send header to the client*/
		if( 0 > (fildes = content_get(context->path))) {
			gfs_sendheader(context->ctx, GF_FILE_NOT_FOUND, 0);
            exit(EXIT_FAILURE);
        }

		/* Calculating the file size */
		file_len = lseek(fildes, 0, SEEK_END);

		gfs_sendheader(context->ctx, GF_OK, file_len);

		/* Sending the file contents chunk by chunk. */
		context->bytes_transferred = 0;
		while(context->bytes_transferred < file_len) {
			read_len = pread(fildes, buffer, BUFFER_SIZE, context->bytes_transferred);
			if (read_len <= 0) {
				fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len,
						context->bytes_transferred, file_len );
				gfs_abort(context->ctx);
				exit(EXIT_FAILURE);
			}
			write_len = gfs_send(context->ctx, buffer, read_len);
			if (write_len != read_len) {
				fprintf(stderr, "handle_with_file write error");
				gfs_abort(context->ctx);
				exit(EXIT_FAILURE);
			}
			context->bytes_transferred += write_len;
		}
	}
	pthread_exit(NULL);
}

/**
 * Empties out the queue and frees the space allocated to it
 */
void global_cleanup() {
	steque_destroy(context_queue);
	free(context_queue);
}
