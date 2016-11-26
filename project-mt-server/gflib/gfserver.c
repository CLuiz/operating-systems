#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>

#include "gfserver.h"

/*
 * Modify this file to implement the interface specified in
 * gfserver.h.
 */

// structure for get file server
typedef struct gfserver_t {
	unsigned short portno;
	int max_npending;
	ssize_t (*handler)(gfcontext_t *, char *, void*);
	void* handlerarg;
} gfserver_t;

// structure for get file context
typedef struct gfcontext_t {
	int socket_fd;
} gfcontext_t;

/*
 * Sends to the client the Getfile header containing the appropriate
 * status and file length for the given inputs.  This function should
 * only be called from within a callback registered gfserver_set_handler.
 * @param ctx - pointer to gfcontext_t client context
 * @param status - status to client request
 * @param file_len - size of file being requested
 * @return status of send
 */
ssize_t gfs_sendheader(gfcontext_t *ctx, gfstatus_t status, size_t file_len){
	char header[100];
	char status_string[20];

	// convert the status to the appropriate string
	switch(status) {
		case GF_OK:
			strcpy(status_string, "OK");
			sprintf(header, "GETFILE %s %zu\r\n\r\n", status_string, file_len);
			break;
		case GF_FILE_NOT_FOUND:
			strcpy(status_string, "FILE_NOT_FOUND");
			sprintf(header, "GETFILE %s \r\n\r\n", status_string);
			break;
		default:
			strcpy(status_string, "ERROR");
			sprintf(header, "GETFILE %s \r\n\r\n", status_string);
	}

	// send to client
	return send(ctx->socket_fd, header, strlen(header), 0);
}

/*
 * Sends size bytes starting at the pointer data to the client
 * This function should only be called from within a callback registered
 * with gfserver_set_handler.  It returns once the data has been
 * sent.
 * @param ctx - pointer to gfcontext_t client context
 * @param data - data to send
 * @param len - size of data
 * @return status of send
 */
ssize_t gfs_send(gfcontext_t *ctx, void *data, size_t len){
	return send(ctx->socket_fd, data, len, 0);
}

/*
 * Aborts the connection to the client associated with the input
 * gfcontext_t.
 * @param gfcontext_t - client context to abord
 */
void gfs_abort(gfcontext_t *ctx){
	close(ctx->socket_fd);
}

/*
 * This function must be the first one called as part of
 * setting up a server.  It returns a gfserver_t handle which should be
 * passed into all subsequent library calls of the form gfserver_*.  It
 * is not needed for the gfs_* call which are intended to be called from
 * the handler callback.
 * @return gfserver_t with necessary memory allocated
 */
gfserver_t* gfserver_create(){
	gfserver_t *gfs = malloc(sizeof(gfserver_t));
	gfs->portno = 0;
	gfs->max_npending = 1;
	gfs->handler = NULL;
	gfs->handlerarg = NULL;
	return gfs;
}

/*
 * Sets the port at which the server will listen for connections.
 * @param gfs - pointer to gfcserver_t
 * @param port - port number
 */
void gfserver_set_port(gfserver_t *gfs, unsigned short port){
	if (gfs != NULL)
		gfs->portno = port;
}

/*
 * Sets the maximum number of pending connections which the server
 * will tolerate before rejecting connection requests.
 * @param gfs - pointer to gfcserver_t
 * @param int - max pending connections
 */
void gfserver_set_maxpending(gfserver_t *gfs, int max_npending){
	if (gfs != NULL)
		gfs->max_npending = max_npending;
}

/*
 * Sets the handler callback, a function that will be called for each each
 * request.  As arguments, the receives
 * - a gfcontext_t handle which it must pass into the gfs_* functions that
 * 	 it calls as it handles the response.
 * - the requested path
 * - the pointer specified in the gfserver_set_handlerarg option.
 * The handler should only return a negative value to signal an error.
 * @param gfs - pointer to gfcserver_t
 * @param handler to be used
 */
void gfserver_set_handler(gfserver_t *gfs, ssize_t (*handler)(gfcontext_t *, char *, void*)){
	if (gfs != NULL)
		gfs->handler = handler;
}

/*
 * Sets the third argument for calls to the handler callback.
 * @param gfs - pointer to gfcserver_t
 * @param handlerarg to be used
 */
void gfserver_set_handlerarg(gfserver_t *gfs, void* arg){
	if (gfs != NULL)
		gfs->handlerarg = arg;
}

/*
 * Starts the server.  Does not return.
 * @param gfs - server params utilized
 */
void gfserver_serve(gfserver_t *gfs){
	struct sockaddr_in server, client;
	int server_socket_fd = 0;
	int set_reuse_addr = 1;
	char client_buffer[4096];
	char temp_buffer[4096];
	char client_path[100];
	socklen_t client_addr_len = sizeof(client);
	size_t file_block_size;
	int extract_status = -1;
	int handle_status = -1;
	int attempts;

	// configure server
	bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(gfs->portno);

	// setup socket, bind and listen
	server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &set_reuse_addr, sizeof(set_reuse_addr));
	bind(server_socket_fd, (struct sockaddr *)&server, sizeof(server));
	listen(server_socket_fd, gfs->max_npending);

	// accept client requests
	while (1) {
		gfcontext_t *context = malloc(sizeof(gfcontext_t));
		context->socket_fd = accept(server_socket_fd, (struct sockaddr *)&client, &client_addr_len);

		// prepate client buffer
		memset(client_buffer, '\0', sizeof(client_buffer));
		strcpy(client_buffer, "");
		attempts = 0;

		// receive client request and extract path
		do {
			memset(temp_buffer, '\0', sizeof(temp_buffer));
			file_block_size = read(context->socket_fd , temp_buffer, sizeof(temp_buffer));
			printf("file size: %zu\n", file_block_size);
			// if everything was not received in one chunk keep adding on and looping
			strcat(client_buffer, temp_buffer);
			attempts = attempts + 1;
		} while (strstr(client_buffer, "\r\n\r\n") == NULL && file_block_size > 0 && attempts < 10);

		if (strstr(client_buffer, "\r\n\r\n") == NULL || strstr(client_buffer, "GETFILE GET") == NULL)
			gfs_sendheader(context, GF_FILE_NOT_FOUND, 0);

		// if file request was received try to extract path
		if (file_block_size > 0) {
			memset(client_path, '\0', sizeof(client_path));
			extract_status = sscanf(client_buffer, "GETFILE GET %s\r\n\r\n", client_path);
		}

		// if path was extracted successfully then send to handler
		if (extract_status > 0) {
			handle_status = gfs->handler(context, client_path, gfs->handlerarg);
			extract_status = -1;
		}

		if (handle_status < 0)
		    gfs_abort(context);
	}
}
