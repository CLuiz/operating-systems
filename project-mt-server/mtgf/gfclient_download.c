#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>

#include "workload.h"
#include "gfclient.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webclient [options]\n"                                                     \
"options:\n"                                                                  \
"  -s [server_addr]    Server address (Default: 0.0.0.0)\n"                   \
"  -p [server_port]    Server port (Default: 8888)\n"                         \
"  -w [workload_path]  Path to workload file (Default: workload.txt)\n"       \
"  -t [nthreads]       Number of threads (Default 1)\n"                       \
"  -n [num_requests]   Requests download per thread (Default: 1)\n"           \
"  -h                  Show this help message\n"                              \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"server",        required_argument,      NULL,           's'},
  {"port",          required_argument,      NULL,           'p'},
  {"workload-path", required_argument,      NULL,           'w'},
  {"nthreads",      required_argument,      NULL,           't'},
  {"nrequests",     required_argument,      NULL,           'n'},
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};

// request params to be passed to pthread when created
typedef struct request_params {
    char *server;
    unsigned short portno;
    int num_requests;
    char *path;
} request_params;

void *connection_handler(request_params *req);

static void Usage() {
	fprintf(stdout, "%s", USAGE);
}

static void localPath(char *req_path, char *local_path){
  static int counter = 0;

  sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

static FILE* openFile(char *path){
  char *cur, *prev;
  FILE *ans;

  /* Make the directory if it isn't there */
  prev = path;
  while(NULL != (cur = strchr(prev+1, '/'))){
    *cur = '\0';

    if (0 > mkdir(&path[0], S_IRWXU)){
      if (errno != EEXIST){
        perror("Unable to create directory");
        exit(EXIT_FAILURE);
      }
    }

    *cur = '/';
    prev = cur;
  }

  if( NULL == (ans = fopen(&path[0], "w"))){
    perror("Unable to open file");
    exit(EXIT_FAILURE);
  }

  return ans;
}

/* Callbacks ========================================================= */
static void writecb(void* data, size_t data_len, void *arg){
  FILE *file = (FILE*) arg;

  fwrite(data, 1, data_len, file);
}

/* Main ========================================================= */
int main(int argc, char **argv) {
/* COMMAND LINE OPTIONS ============================================= */
  char *server = "localhost";
  unsigned short port = 8888;
  char *workload_path = "workload.txt";
  int i;
  int option_char = 0;
  int nrequests = 1;
  int nthreads = 1;

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "s:p:w:n:t:h", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 's': // server
        server = optarg;
        break;
      case 'p': // port
        port = atoi(optarg);
        break;
      case 'w': // workload-path
        workload_path = optarg;
        break;
      case 'n': // nrequests
        nrequests = atoi(optarg);
        break;
      case 't': // nthreads
        nthreads = atoi(optarg);
        break;
      case 'h': // help
        Usage();
        exit(EXIT_SUCCESS);
        break;
      default:
        Usage();
        exit(EXIT_FAILURE);
    }
  }

  if (EXIT_SUCCESS != workload_init(workload_path)){
    fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
    exit(EXIT_FAILURE);
  }

  // define pthreads and other globals
  pthread_t thread[nthreads];
  gfc_global_init();

  /*Making the requests...*/
  fprintf(stdout, "Creating %d threads with %d requests.\n", nthreads, nrequests);
  for(i = 0; i < nthreads; i++){
      // initiate request params
      request_params *req = malloc(sizeof(request_params));
      req->server = server;
      req->portno = port;
      req->num_requests = nrequests;
      req->path = workload_get_path();

      // create pthread
      if (pthread_create(&(thread[i]), NULL, (void *)connection_handler, req) < 0)
            fprintf(stderr, "Thread %d was unable to create.\n", i);
  }

  // close all threads
  for (i = 0; i < nthreads; i++)
      pthread_join(thread[i], NULL);

  // clean up and exit
  fprintf(stdout, "All threads and requests complete.\n");
  gfc_global_cleanup();
  return EXIT_SUCCESS;
}

/**
 * Hanlder for pthread when created.  Processes the request with the given
 * client request structure
 * @param
 */
void *connection_handler(request_params *req) {

    // initialize all local variables needed
    int i;
    char local_path[512];
    FILE *file;
    gfcrequest_t *gfr;
    int returncode = -1;

    for (i = 0; i < req->num_requests; i++) {
        // get request path for file and ensure its useable
        req->path = workload_get_path();
        if(strlen(req->path) > 256) {
          fprintf(stderr, "Request path exceeded maximum of 256 characters\n.");
          exit(EXIT_FAILURE);
        }

        // get file informaiton and open
        localPath(req->path, local_path);
        file = openFile(local_path);

        // set request params to client request structure
        gfr = gfc_create();
        gfc_set_server(gfr, req->server);
        gfc_set_path(gfr, req->path);
        gfc_set_port(gfr, req->portno);
        gfc_set_writefunc(gfr, writecb);
        gfc_set_writearg(gfr, file);

        // initiate structure and begin receiving file
        fprintf(stdout, "Requesting %s%s\n", req->server, req->path);
        if ((returncode = gfc_perform(gfr) < 0)) {
          fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
          fclose(file);
          if ( 0 > unlink(local_path))
            fprintf(stderr, "unlink failed on %s\n", local_path);
        }
        else
            fclose(file);

        // if request didnt' complete successfully unlink file
        if ( gfc_get_status(gfr) != GF_OK) {
          if ( 0 > unlink(local_path))
            fprintf(stderr, "unlink failed on %s\n", local_path);
        }

        fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(gfr)));
        fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(gfr), gfc_get_filelen(gfr));
  }

  // exit thread and unallocate memory for request_params
  free(req);
  pthread_exit(NULL);
}
