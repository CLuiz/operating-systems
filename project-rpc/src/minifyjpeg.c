#include <stdio.h>
#include "minifyjpeg.h"
#include "magickminify.h"

/* Implement the needed server-side functions here */

/**
 * Method used to minify a requested image
 */
bool_t minify_image_1_svc(image source, image *result, struct svc_req *request) {
    ssize_t result_len;

    // initialize magic minfiy library
    magickminify_init();

    // minify image
    printf("Beginning minify process\n");
    result->buffer.buffer_val = magickminify(
            source.buffer.buffer_val, source.buffer.buffer_len, &result_len);

    // assign the result length
    result->buffer.buffer_len = (u_int) result_len;
    printf("Minify process complete\n");

    return 1;
}

/**
 * Clean up after complete
 */
int minifiy_program_1_freeresult (SVCXPRT *svcxprt, xdrproc_t xdr_proc, caddr_t xdr_caddr) {
    // clean up magick minify library
    magickminify_cleanup();

    // free xdr procedure
    xdr_free(xdr_proc, xdr_caddr);

    return 1;
}