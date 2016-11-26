#include <stdlib.h>
#include <stdio.h>
#include "minifyjpeg_xdr.c"
#include "minifyjpeg_clnt.c"
#include "minifyjpeg.h"

/**
 * Connects to RPC server and return pointer to client
 */
CLIENT* get_minify_client(char *server){
    CLIENT *cl;

    /* Your code here */
    if ((cl = clnt_create(server, MINIFIY_PROGRAM, MINIFY_VERSION, "tcp")) == NULL) {
        printf("Unable to connect to server\n");
        clnt_pcreateerror (server);
        exit(EXIT_FAILURE);
    }

    return cl;
}

/**
 * Calls remote procedure on the client
 */
void* minify_via_rpc(CLIENT *cl, void* src_val, size_t src_len, size_t *dst_len){

	/*Your code here */

    // initialize image to minify
    image src_img;
    src_img.buffer.buffer_len = (u_int) src_len;
    src_img.buffer.buffer_val = src_val;

    // initialize minified image
    image *min_img = malloc(sizeof(image));
    min_img->buffer.buffer_val = (char *) malloc(src_len * sizeof(char));

    // make remote procedure call to minify image
    enum clnt_stat rpc_result = minify_image_1(src_img, min_img, cl);
    if (rpc_result == RPC_FAILED) {
        clnt_perrno(rpc_result);
        clnt_perror(cl, "Minfication failed");
    }

    // set variables for minified image and return buffer
    *dst_len = min_img->buffer.buffer_len;
    return min_img->buffer.buffer_val;
}