#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>

#include "gfserver.h"

/**
* Memory structure for cURL to write in chunks
*/
struct memory_struct {
    char *memory;
    size_t size;
};

/**
* Write callback function can be used to download data into a chunk of memory
* instead of storing it in a file.
*/
static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    // get real size of memory and setup memory structure
    size_t realsize = size * nmemb;
    struct memory_struct *mem = (struct memory_struct *)userp;

    // allocate memory and test if enough space
    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if(mem->memory == NULL) {

        // not enough memory to allocate, exit
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    // copy to memory the contents
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}


/*
* Worker function for handling a request with cURL.
*/
ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg) {
    // initialize all variables needed
    size_t bytes_remaining;
    size_t bytes_transferred = 0;
    ssize_t bytes_written;
    long response_code;
    char buffer[4096];
    char *data_dir = arg;
    CURL *curl_handle;
    CURLcode response;
    struct memory_struct chunk;

    // allocate nothing and no data yet received
    chunk.memory = malloc(1);
    chunk.size = 0;

    // copy the directory and path to the buffer
    strcpy(buffer,data_dir);
    strcat(buffer,path);

    // initialize curl handle
    curl_handle = curl_easy_init();

    // set options for curl
    curl_easy_setopt(curl_handle, CURLOPT_URL, buffer);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &chunk);

    // perform curl request and cleanup
    response = curl_easy_perform(curl_handle);
    curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_cleanup(curl_handle);

    // if error received from cURL then return error
    if (response != CURLE_OK) {
        printf("Unable to process curl %s\n", curl_easy_strerror(response));
        free(chunk.memory);
        return gfs_sendheader(ctx, GF_ERROR, 0);
    }

    // if HTTP response is 400 or more than return not found
    if (response_code >= 400) {
        printf("Error, received response code %zu\n", response_code);
        free(chunk.memory);
        return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
    }

    // send header and loop until data sent
    gfs_sendheader(ctx, GF_OK, chunk.size);
    while(bytes_transferred < chunk.size){

        // calculate how much is being sent this time
        bytes_remaining = chunk.size - bytes_transferred;
        bytes_remaining = (4096 > bytes_remaining) ? bytes_remaining : 4096;

        // attempt to write the file and check to ensure it was successful
        bytes_written = gfs_send(ctx, &(chunk.memory[bytes_transferred]) , bytes_remaining);
        if (bytes_written != bytes_remaining) {
            printf("Write error - writing: %zd, written: %zu, total size: %zu\n",
                   bytes_written, bytes_transferred, chunk.size);
            free(chunk.memory);
            return EXIT_FAILURE;
        }

        // update the bytes transferred
        bytes_transferred += bytes_written;
    }

    // free up memory and exit with status from processing
    free(chunk.memory);
    return EXIT_SUCCESS;
}
