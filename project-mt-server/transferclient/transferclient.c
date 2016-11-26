#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>

#define BUFSIZE 4096

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  transferclient [options]\n"                                                \
"options:\n"                                                                  \
"  -s                  Server (Default: localhost)\n"                         \
"  -p                  Port (Default: 8888)\n"                                \
"  -o                  Output file (Default foo.txt)\n"                       \
"  -h                  Show this help message\n"

/* Main ========================================================= */
int main(int argc, char **argv) {
	int option_char = 0;
	char *hostname = "localhost";
	unsigned short portno = 8888;
	char *filename = "foo.txt";

	// Parse and set command line arguments
	while ((option_char = getopt(argc, argv, "s:p:o:h")) != -1) {
		switch (option_char) {
			case 's': // server
				hostname = optarg;
				break;
			case 'p': // listen-port
				portno = atoi(optarg);
				break;
			case 'o': // filename
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
	int socket_fd = 0; // socket file descriptor
	struct sockaddr_in server_socket_addr; // address of server
	char buffer[BUFSIZE];  // string buffer and max size
	int num_bits;  // number of bits received
	char *current_dir;  // current directory to write file to
	FILE *file;  // file pointer
	size_t file_block_size;  // file block size

	// get host information
	struct hostent *server = gethostbyname(hostname);
	unsigned long server_addr_nbo = *(unsigned long *)(server->h_addr_list[0]);

	// create socket
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		fprintf(stderr, "Failed to create socket\n");
		exit(1);
	}

	// configure socket address
	bzero(&server_socket_addr, sizeof(server_socket_addr));
	server_socket_addr.sin_family = AF_INET;
	server_socket_addr.sin_port = htons(portno);
	server_socket_addr.sin_addr.s_addr = server_addr_nbo;

	// attempt connection to socket
	if (connect(socket_fd, (struct sockaddr *)&server_socket_addr, sizeof(server_socket_addr)) < 0) {
		fprintf(stderr, "Failed to connect to %s:%d!\n", hostname, portno);
		close(socket_fd);
		exit(1);
	} else
		fprintf(stdout, "Connected to %s:%d!\n", hostname, portno);


	// set file location
	current_dir = getcwd(buffer, BUFSIZE);
	strcat(strcat(current_dir, "/"), filename);

	// open file location
	file = fopen(current_dir, "a");
	if (file == NULL) {
		fprintf(stderr, "Failed to open file %s.\n", filename);
		close(socket_fd);
		exit(1);
	}

	// read in file as much as buffer size allows
	bzero(buffer, BUFSIZE);
	while((file_block_size = recv(socket_fd, buffer, BUFSIZE, 0)) > 0) {
		num_bits = num_bits + (int) file_block_size;
		if (fwrite(buffer, sizeof(char), file_block_size, file) < file_block_size) {
			fprintf(stderr, "File failed to write.\n");
			close(socket_fd);
			exit(1);
		}
	}

	// close everything and exit
	fclose(file);
	printf("Wrote file \"%s\" from server.\n", filename);
	printf("File size %d\n", num_bits);
	close(socket_fd);
	printf("Connection closed.\n");
	return 0;
}
