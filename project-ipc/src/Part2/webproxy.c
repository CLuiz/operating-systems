#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

#include "gfserver.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webproxy [options]\n"                                                      \
"options:\n"                                                                  \
"  -n [segment-count]  Num segments for cache data transfer (Default: 10)"    \
"  -z [segment-size]   Size of segments for cache data transfer (Default: 1024)"\
"  -p [listen_port]    Listen port (Default: 8888)\n"                         \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"      \
"  -s [server]         The server to connect to (Default: Udacity S3 instance)"\
"  -h                  Show this help message\n"                              \
"special options:\n"                                                          \
"  -d [drop_factor]    Drop connects if f*t pending requests (Default: 5).\n"


/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
        {"segment-count", required_argument,      NULL,           'n'},
        {"segment-size",  required_argument,      NULL,           'z'},
        {"port",          required_argument,      NULL,           'p'},
        {"thread-count",  required_argument,      NULL,           't'},
        {"server",        required_argument,      NULL,           's'},
        {"help",          no_argument,            NULL,           'h'},
        {NULL,            0,                      NULL,             0}
};

extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);

extern void cache_init(int nsegments, int size_seg);
extern void cache_cleanup();

static gfserver_t gfs;

static void _sig_handler(int signo){
    if (signo == SIGINT || signo == SIGTERM){
        fprintf(stderr, "\n\nWebproxy stopping\n\n");

        gfserver_stop(&gfs);
        cache_cleanup();
        exit(signo);
    }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
    int i, option_char = 0;
    unsigned short port = 8888;
    unsigned short nworkerthreads = 1;
    char *server = "s3.amazonaws.com/content.udacity-data.com";
    int nsegments = 1;
    int size_seg = 4096;

    if (signal(SIGINT, _sig_handler) == SIG_ERR){
        fprintf(stderr,"Can't catch SIGINT...exiting.\n");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGTERM, _sig_handler) == SIG_ERR){
        fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
        exit(EXIT_FAILURE);
    }

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "n:z:p:t:s:h", gLongOptions, NULL)) != -1) {
        switch (option_char) {
            case 'n':
                nsegments = atoi(optarg);
                break;
            case 'z':
                size_seg = atoi(optarg);
                break;
            case 'p': // listen-port
                port = (unsigned short) atoi(optarg);
                break;
            case 't': // thread-count
                nworkerthreads = (unsigned short) atoi(optarg);
                break;
            case 's': // file-path
                server = optarg;
                break;
            case 'h': // help
                fprintf(stdout, "%s", USAGE);
                exit(0);
            default:
                fprintf(stderr, "%s", USAGE);
                exit(1);
        }
    }

    /* SHM initialization...*/
    cache_init(nsegments, size_seg);

    /*Initializing server*/
    gfserver_init(&gfs, nworkerthreads);

    /*Setting options*/
    gfserver_setopt(&gfs, GFS_PORT, port);
    gfserver_setopt(&gfs, GFS_MAXNPENDING, 10);
    gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);
    for(i = 0; i < nworkerthreads; i++)
        gfserver_setopt(&gfs, GFS_WORKER_ARG, i, server);

    /*Loops forever*/
    gfserver_serve(&gfs);
}