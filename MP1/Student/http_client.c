#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// my imports
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

// beej's
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void *get_in_addr(struct sockaddr *sa);
void send_GET(char *remote, int sock_fd);
void handle_GET(int sock_fd, char*local);
void handle_server_response(char *method, char *local, int sock_fd);
size_t binary_data_network_read(int in_fd, int out_fd);
size_t network_read(int sock_fd, void *buffer, size_t buffer_size);
size_t network_read_one_line(int sock_fd, char *buffer, size_t buffer_size);
int connect_to_server(char *host, char *port);

static FILE *output_fp;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char **argv) {
    output_fp = fopen("output", "w+");

    // first argument
    char *input = argv[1];
    fprintf(stderr, "input: %s\n", input);

    // protocol specified is not HTTP    
    char *HTTP_PROTOCOL = "http://";
    if (strncmp(HTTP_PROTOCOL, input, strlen(HTTP_PROTOCOL) != 0)) {
        fprintf(output_fp, "INVALIDPROTOCOL");
    }    

    //get hostname & port
    char *host_port_path = input + strlen(HTTP_PROTOCOL);
    char *host_port_endptr = strchr(host_port_path, '/');
    char *host_port = strndup(host_port_path, host_port_endptr - host_port_path); // NEW
    char *host = NULL;
    char *port = NULL;
    char *host_endptr = strchr(host_port, ':');
    if (host_endptr) {
        host = strndup(host_port, host_endptr - host_port); // NEW
        port = port = strndup(host_endptr + 1, host_port_endptr - host_endptr); // NEW
    } else {
        host = host_port;
    }

    // connext to server
    int sock_fd = connect_to_server(host, port);
    fprintf(stderr, "socket fd: %d\n", sock_fd);

    close(sock_fd);
    fclose(output_fp);
}

int connect_to_server(char *host, char *port) {
    fprintf(stderr, "hostname: %s\n", host);
    fprintf(stderr, "port: %s\n", port);

    int sockfd; 
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(output_fp, "NOCONNECTION");
		exit(1);
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	fprintf(stderr, "client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

    return sockfd;
}
