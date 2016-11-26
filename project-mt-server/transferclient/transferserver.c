#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#if 0
/*
 * Structs exported from netinet/in.h (for easy reference)
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr;
};

/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
  char    *h_name;        /* official name of host */
  char    **h_aliases;    /* alias list */
  int     h_addrtype;     /* host address type */
  int     h_length;       /* length of address */
  char    **h_addr_list;  /* list of addresses */
}
#endif

#define BUFSIZE 4096

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  transferserver [options]\n"                                                \
"options:\n"                                                                  \
"  -p                  Port (Default: 8888)\n"                                \
"  -f                  Filename (Default: bar.txt)\n"                         \
"  -h                  Show this help message\n"

/**
 * Listens for incoming messages.
 **/
void listen_incoming_message(int *socket_fd, char *filename) {
  char buffer[BUFSIZE]; // buffer string and max size to handle
  int client_socket_fd;  // client socket file descriptor
  struct sockaddr_in client;  // client address
  socklen_t client_addr_len;  // length of client address
  char *current_dir;  // current directory to write file to
	FILE *file;  // filename pointer
	size_t file_block_size;  // file block size
  int num_bits;

  // Accept a new client
  client_addr_len = sizeof(client);
  client_socket_fd = accept(*socket_fd, (struct sockaddr *)&client, &client_addr_len);
  if (client_socket_fd < 0)
    fprintf(stderr, "Did not receive client\n");

  // set file location
  current_dir = getcwd(buffer, BUFSIZE);
  strcat(strcat(current_dir, "/"), filename);

  // open file location
	file = fopen(current_dir, "r");
	if (file == NULL) {
		fprintf(stderr, "Failed to open file %s.\n", filename);
		close(client_socket_fd);
		exit(1);
	}


  // read file and send it as much as buffer size allows
	bzero(buffer, BUFSIZE);
	while((file_block_size = fread(buffer, sizeof(char), BUFSIZE, file)) > 0) {
		if (send(client_socket_fd, buffer, file_block_size, 0) < 0) {
			fprintf(stderr, "File failed to send.\n");
      fclose(file);
			close(client_socket_fd);
			exit(1);
		}
    num_bits = num_bits + (int) file_block_size;
	}


  // close socket when complete
  fclose(file);
  printf("Sent file \"%s\" from server.\n", filename);
  close(client_socket_fd);
  printf("Client connection closed.\n");
}

int main(int argc, char **argv) {
  int option_char;
  int portno = 8888; /* port to listen on */
  char *filename = "bar.txt"; /* file to transfer */

  // Parse and set command line arguments
  while ((option_char = getopt(argc, argv, "p:f:h")) != -1){
    switch (option_char) {
      case 'p': // listen-port
        portno = atoi(optarg);
        break;
      case 'f': // listen-port
        filename = optarg;
        break;
      case 'h': // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
      default:
        fprintf(stderr, "%s", USAGE);
        exit(1);
    }
  }

  /* Socket Code Here */
  int socket_fd = 0;  // socket file descriptor
  struct sockaddr_in server; // server socket address`
  int set_reuse_addr = 1; // socket reuse option 1 == ON
  int maxnpending = 5;  // max pending

  // create socket
  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		fprintf(stderr, "Failed to create socket\n");
		exit(1);
	}

  // setup socket host
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &set_reuse_addr, sizeof(set_reuse_addr)) != 0)
    fprintf(stderr, "Failed to setup address reuse\n");

  // configure server socket structure
  bzero(&server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(portno);

  // bind the server Socket
  if (bind(socket_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
    fprintf(stderr, "Failed to bind socket\n");
    exit(1);
  }

  // listen on the socket
  if (listen(socket_fd, maxnpending) < 0) {
    fprintf(stderr, "Listener failed to start\n");
    exit(1);
  } else
    fprintf(stdout, "Transfer Server listening on port: %d\n", portno);

    // handle incoming client connection
    while (1)
      listen_incoming_message(&socket_fd, filename);

    return 0;
}
