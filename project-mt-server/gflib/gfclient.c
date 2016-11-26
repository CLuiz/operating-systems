#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>

#include "gfclient.h"

#define BUFSIZE 4096

// helper function for getting status integer
int gfc_intstatus(char *status);

// file request structure
typedef struct gfcrequest_t{
    char *server;
    char *path;
    unsigned short portno;
    void (*headerfunc)(void*, size_t, void *);
    void *headerarg;
    void (*writefunc)(void*, size_t, void *);
    void *writearg;
    gfstatus_t status;
    size_t bytesreceived;
	size_t filelen;
} gfcrequest_t;

/**
* This function must be the first one called as part of
* request.  It returns a gfcrequest_t handle which should be
* passed into all subsequent library calls pertaining to
* this requeest.
* @return request - get file request
*/
gfcrequest_t *gfc_create(){
    struct gfcrequest_t *request;
    request = (gfcrequest_t *)malloc(sizeof(gfcrequest_t ));
    bzero(request, sizeof(gfcrequest_t));
    request->server = NULL;
    request->path = NULL;
    request->portno = 0;
    request->headerfunc = NULL;
	request->headerarg = NULL;
	request->writefunc = NULL;
	request->writearg = NULL;
    request->status = GF_OK;
	request->bytesreceived = 0;
	request->filelen = 0;
    return request;
}

/**
 * Sets the server to which the request will be sent.
 * @param gfr - pointer to gfcrequest_t
 * @param server - character string for server
 */
void gfc_set_server(gfcrequest_t *gfr, char* server){
    if (gfr != NULL && server != NULL) {
        gfr->server = (char *)calloc(strlen(server) + 1, sizeof(char));
        strcpy(gfr->server, server);
    }
}

/**
 * Sets the path of the file that will be requested.
 * @param gfr - pointer to gfcrequest_t
 * @param path - character string for path
 */
void gfc_set_path(gfcrequest_t *gfr, char* path){
    if (gfr != NULL && path != NULL) {
        gfr->path = (char *)calloc(strlen(path) + 1, sizeof(char));
        strcpy(gfr->path, path);
    }
}

/**
 * Sets the port over which the request will be made.
 * @param gfr - pointer to gfcrequest_t
 * @param port - port number
 */
void gfc_set_port(gfcrequest_t *gfr, unsigned short port){
    if (gfr != NULL)
        gfr->portno = port;
}

/**
* Sets the callback for received header.  The registered callback
* will receive a pointer the header of the response, the length
* of the header response as it's second argument (don't assume that
* this is null-terminated), and the pointer registered with
* gfc_set_headerarg (or NULL if not specified) as the third argument.
*
* You may assume that the callback will only be called once and will
* contain the full header.
 * @param gfr - pointer to gfcrequest_t
 * @param headerfunc - pointer to headerfunction used
 */
void gfc_set_headerfunc(gfcrequest_t *gfr, void (*headerfunc)(void*, size_t, void *)){
    if (gfr != NULL && headerfunc != NULL)
        gfr->headerfunc = headerfunc;
}

/**
 * Sets the third argument for all calls to the registered header callback.
 * @param gfr - pointer to gfcrequest_t
 * @param headerarg - pointer to the header args
 */
void gfc_set_headerarg(gfcrequest_t *gfr, void *headerarg){
    if (gfr != NULL && headerarg != NULL)
        gfr->headerarg = headerarg;
}

/**
* Sets the callback for received chunks of the body.  The registered
* callback will receive a pointer the chunk, the length of the chunk
* as it's second argument (don't assume that this is null-terminated),
* and the pointer registered with gfc_set_writearg (or NULL if not
* specified) as the third argument.
*
* The callback may be called multiple times in a single request.  The
* gfclient library does not store the entire contents of the requested file
* but rather calls this callback each time that it receives a chunk of data
* from the server.
 * @param gfr - pointer to gfcrequest_t
 * @param headerfunc - pointer to the write function used
 */
void gfc_set_writefunc(gfcrequest_t *gfr, void (*writefunc)(void*, size_t, void *)){
    if (gfr != NULL && writefunc != NULL)
        gfr->writefunc = writefunc;
}

/**
 * Sets the third argument for all calls to the registered header callback.
 * @param gfr - pointer to gfcrequest_t
 * @param headerfunc - pointer to the write args
 */
void gfc_set_writearg(gfcrequest_t *gfr, void *writearg){
    if (gfr != NULL && writearg != NULL)
        gfr->writearg = writearg;
}

/*
 * Performs the transfer as described in the options.  Returns a value of 0
 * if the communication is successful, including the case where the server
 * returns a response with a FILE_NOT_FOUND or ERROR response.  If the
 * communication is not successful (e.g. the connection is closed before
 * transfer is complete or an invalid header is returned), then a negative
 * integer will be returned.
 * @param gfr - pointer to gfcrequest_t
 * @return integer based on result of function
 */
int gfc_perform(gfcrequest_t *gfr){
    int EXIT_ERROR = -1;

    if (gfr == NULL) {
        fprintf(stderr, "[Client] Null client request given.\n");
        return EXIT_ERROR;
    }

    // initialize variables
    int socket_fd;
    struct hostent *server;
    unsigned long server_addr_nbo;
    struct sockaddr_in server_socket_addr;
    char full_request[200];
    char *scheme, *method, *marker;
    scheme = "GETFILE", method = "GET", marker = "\r\n\r\n";
    char buffer[BUFSIZE], header_buffer[20];
    char *content_buffer;
    char status[50];
    size_t file_block_size, filelen;
    int header_processed = 0;
    size_t data_written = 0;
    filelen = 1;
    int attempt_count = 0;

    // configure socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        fprintf(stderr, "[Client] Failed to create socket.\n");
        gfr->status = GF_ERROR;
        return EXIT_ERROR;
    }

    // configure server
    server = gethostbyname(gfr->server);
	server_addr_nbo = *(unsigned long *)(server->h_addr_list[0]);
	bzero(&server_socket_addr, sizeof(server_socket_addr));
	server_socket_addr.sin_family = AF_INET;
	server_socket_addr.sin_port = htons(gfr->portno);
	server_socket_addr.sin_addr.s_addr = server_addr_nbo;

    // attempt connection to socket
    if (connect(socket_fd, (struct sockaddr *)&server_socket_addr, sizeof(server_socket_addr)) < 0) {
        fprintf(stderr, "[Client] Failed to connect to %s:%d!\n", gfr->server, gfr->portno);
        gfr->status = GF_ERROR;
        return EXIT_ERROR;
    }

    // setup initial request
    memset(full_request, '\0', sizeof(full_request));
    sprintf(full_request, "%s %s %s%s", scheme, method, gfr->path, marker);

    // request from server
    if (send(socket_fd, full_request, strlen(full_request), 0) < 0) {
        fprintf(stderr, "[Client] Received no response from %s!\n", gfr->server);
        gfr->status = GF_ERROR;
        return EXIT_ERROR;
    }

	// wait until response is received from server
    while (filelen - data_written > 0) {
        // received data and set initial variables
        file_block_size = recv(socket_fd, buffer, BUFSIZE, 0);
        gfr->bytesreceived = gfr->bytesreceived + file_block_size;
        content_buffer = buffer;

        // check if any header has been processed yet
    	if(header_processed <= 0) {
            memset(header_buffer, '\0', sizeof(header_buffer));

            // check if any data received or if the marker exists
            if (gfr->bytesreceived == 0) {
                gfr->status = GF_FILE_NOT_FOUND;
                break;
            }
            else if (strstr(buffer, marker) == NULL || strstr(buffer, scheme) == NULL) {
                gfr->status = GF_INVALID;
                break;
            }

            // try to scan the correct format
            header_processed = sscanf(buffer, "GETFILE %s %zu\r\n\r\n", status, &filelen);
            if (header_processed == EOF || header_processed < 2) {
                header_processed = sscanf(buffer, "GETFILE %s \r\n\r\n", status);
                gfr->status = gfc_intstatus(status);
                break;
            }

            if (strlen(gfc_strstatus(gfr->status)) == 0) {
                gfr->status = GF_INVALID;
                break;
            }

            // build the head buffer
            sprintf(header_buffer, "GETFILE %s %zu\r\n\r\n", status, filelen);
            //gfr->headerfunc(header_buffer, sizeof(header_buffer), gfr->headerarg);

            // set the current status and file length
            gfr->status = gfc_intstatus(status);
            gfr->filelen = filelen;

            // correct the file block size
            file_block_size = file_block_size - strlen(header_buffer);
            gfr->bytesreceived = file_block_size;

            // copy over the new content buffer
            content_buffer = buffer + strlen(header_buffer);
    	}

        // if data not being received, attempt 10 times (arbitrary number)
        if (file_block_size <= 0 && attempt_count > 10)
            return EXIT_ERROR;
        else if (file_block_size <= 0)
            attempt_count = attempt_count + 1;

        // write data to file
        if (file_block_size > 0 && gfr->writefunc != NULL && gfr->status == GF_OK) {
            gfr->writefunc(content_buffer, file_block_size, gfr->writearg);
            data_written = data_written + file_block_size;
        }

    }

    // close connection, check for status type to decide what to give back
    close(socket_fd);
    if (gfr->status == GF_OK || gfr->status == GF_FILE_NOT_FOUND ||
        gfr->status == GF_ERROR)
        return EXIT_SUCCESS;
    else
        return EXIT_ERROR;
}

/**
 * Returns the status of the response.
 * @param gfr - pointer to gfcrequest_t
 * @return gfstatus_t - status associated with request
 */
gfstatus_t gfc_get_status(gfcrequest_t *gfr){
    return gfr->status;
}

/*
 * Returns the string associated with the input status
 * @param status - a gfstatus_t showing status of request
 * @return char* of status
 */
char* gfc_strstatus(gfstatus_t status){
    char *statusString = "";

    switch (status) {
        case GF_OK:
            statusString = "OK";
            break;
        case GF_FILE_NOT_FOUND:
            statusString = "FILE_NOT_FOUND";
            break;
        case GF_ERROR:
            statusString = "ERROR";
            break;
        case GF_INVALID:
            statusString = "INVALID";
            break;
        default:
            break;
    }
    return statusString;
}

/*
 * Returns the integer associated with the input status
 * @param status - a string of the status
 * @return int gfc status
 */
int gfc_intstatus(char *status) {
    const char *OK = "OK";
    const char *FILE_NOT_FOUND = "FILE_NOT_FOUND";
    const char *ERROR = "ERROR";
    const char *INVALID = "INVALID";

    if (strcmp(status, OK) == 0)
        return GF_OK;
    if (strcmp(status, FILE_NOT_FOUND) == 0)
        return GF_FILE_NOT_FOUND;
    if (strcmp(status, ERROR) == 0)
        return GF_ERROR;
    if (strcmp(status, INVALID) == 0)
        return GF_INVALID;

    return -1;
}

/**
 * Returns the length of the file as indicated by the response header.
 * Value is not specified if the response status is not OK.
 * @param gfr - pointer to gfcrequest_t
 * @return size_t - size of file requested
 */
size_t gfc_get_filelen(gfcrequest_t *gfr){
    return gfr->filelen;
}

/**
 * Returns actual number of bytes received before the connection is closed.
 * This may be distinct from the result of gfc_get_filelen when the response
 * status is OK but the connection is reset before the transfer is completed.
 * @param gfr - pointer to gfcrequest_t
 * @return size_t - how many bytes received so far
 */
size_t gfc_get_bytesreceived(gfcrequest_t *gfr){
    return gfr->bytesreceived;
}

/*
 * Frees memory associated with the request.
 */
void gfc_cleanup(gfcrequest_t *gfr){
    printf("[Client] Performing cleanup of resources.\n");
    free(gfr);
}

/* Not needed */
void gfc_global_init(){
}

/* Not needed */
void gfc_global_cleanup(){
}
