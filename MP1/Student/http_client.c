// CITATION: several code snippets taken from cs241 Nonstop Networking MP

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
void handle_GET(int sock_fd);
void handle_server_response(int sock_fd);
size_t binary_data_network_read(int in_fd, int out_fd);
size_t network_read_one_line(int sock_fd, char *buffer, size_t buffer_size);
size_t network_write(int sock_fd, void *buffer, size_t len);
int connect_to_server(char *host, char *port);
void print_invalid_response();
void print_connection_closed();
void print_no_connection();

static char *INVALID_PROTOCOL = "INVALIDPROTOCOL";
static char *NO_CONNECTION = "NOCONNECTION";
static char *FILE_NOT_FOUND = "FILENOTFOUND";

int out_fd;
static char *host = NULL;
static char *port = NULL;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char **argv) {
    out_fd = open("output", O_CREAT|O_WRONLY|O_TRUNC, 0777);

    // first argument
    char *input = argv[1];
    // fprintf(stderr, "input: %s\n", input);

    // protocol specified is not HTTP    
    char *HTTP_PROTOCOL = "http://";
    if (strncmp(HTTP_PROTOCOL, input, strlen(HTTP_PROTOCOL)) != 0) {
        fprintf(stderr, "%s\n", INVALID_PROTOCOL);
        write(out_fd, INVALID_PROTOCOL, strlen(INVALID_PROTOCOL));
        exit(1);
    }

    //get hostname & port
    char *host_port_path = input + strlen(HTTP_PROTOCOL);
    char *path = strchr(host_port_path, '/');
    char *host_port = strndup(host_port_path, path - host_port_path); // NEW
    char *host_port_colon = strchr(host_port, ':');
    if (host_port_colon) {
        host = strndup(host_port, host_port_colon - host_port); // NEW
        port = port = strndup(host_port_colon + 1, path - host_port_colon); // NEW
    } else {
        host = host_port;
        port = "80";
    }

    // connext to server
    int sock_fd = connect_to_server(host, port);
    fprintf(stderr, "socket fd: %d\n", sock_fd);

    // send GET request
    send_GET(path, sock_fd);

    // shutdown socket
    shutdown(sock_fd, SHUT_WR);

    handle_server_response(sock_fd);

    close(sock_fd);
    close(out_fd);
}

void send_GET(char *remote, int sock_fd) {
    #define BUFFER_SIZE 1024
    char buffer[BUFFER_SIZE];
    sprintf(buffer, "GET %s HTTP/1.1\r\nHost: %s:%s\r\n\r\n", remote, host, port);
    fprintf(stderr, "request:\n-\n%s\n-\n", buffer);
    ssize_t total = network_write(sock_fd, buffer, strlen(buffer));
    if (total == -1) { print_connection_closed(); exit(1); }
}

void handle_GET(int sock_fd) {
    size_t total = binary_data_network_read(sock_fd, out_fd);
}

void handle_server_response(int sock_fd) {
    #define BUFFER_SIZE 1024
    char buffer[BUFFER_SIZE]; memset(buffer, 0, BUFFER_SIZE);
    network_read_one_line(sock_fd, buffer, BUFFER_SIZE);
    fprintf(stderr, "response status: %s\n", buffer);
    if (strstr(buffer, "200 OK")) {
        while (strcmp(buffer, "\r\n") != 0) {
            memset(buffer, 0, BUFFER_SIZE);
            network_read_one_line(sock_fd, buffer, BUFFER_SIZE);
        }
        handle_GET(sock_fd);
    } else if (strstr(buffer, "404 File not found")) {
        write(out_fd, FILE_NOT_FOUND, strlen(FILE_NOT_FOUND));
    }
}

size_t binary_data_network_read(int in_fd, int out_fd) {
    // CITE: https://github-dev.cs.illinois.edu/angrave/cs241-lectures/blob/master/code/lec25/client.c
    // CITE: https://github-dev.cs.illinois.edu/angrave/cs241-lectures/blob/master/code/lec26/tcpclient.c
    char buffer[1024];
    size_t total = 0;
    while (true) {
        ssize_t bytes = read(in_fd, buffer, sizeof(buffer));
        if (bytes == -1 && errno == EINTR) { continue; }
        if (bytes == -1) { print_invalid_response(); exit(1); }
        if (bytes == 0) { break; }
        total += bytes;
        write(out_fd, buffer, bytes);
    }
    return total;
}

size_t network_read_one_line(int sock_fd, char *buffer, size_t buffer_size) {
    // CITE: https://github-dev.cs.illinois.edu/angrave/cs241-lectures/blob/master/code/lec25/client.c
    // CITE: https://github-dev.cs.illinois.edu/angrave/cs241-lectures/blob/master/code/lec26/tcpclient.c
    size_t idx = 0;
    size_t total = 0;
    while (total < buffer_size) {
        if (idx == buffer_size - 1) { print_invalid_response(); exit(1); }
        ssize_t bytes = read(sock_fd, buffer + total, 1);
        if (bytes == -1 && errno == EINTR) { continue; }
        if (bytes == -1) { print_invalid_response(); exit(1); }
        if (buffer[idx] == '\n') { buffer[idx + 1] = 0; break; }
        total += bytes;
        idx++;
    }
    if (buffer[idx] != '\n') { print_invalid_response(); exit(1); }
    return total;
}

size_t network_write(int sock_fd, void *buffer, size_t len) {
    // CITE: CS 241 Lec 24
    size_t total = 0;
    ssize_t r;
    while ( total < len && (r = write(sock_fd, buffer + total, len - total)) ) {
        if (r == -1 && errno == EINTR) { continue; }
        if (r == -1) {  exit(1); }
        total += r;
    }
    return total;
}

int connect_to_server(char *host, char *port) {
    fprintf(stderr, "host: %s\n", host);
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
        print_no_connection();
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
        print_no_connection();
		exit(1);
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	fprintf(stderr, "client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

    return sockfd;
}

void print_invalid_response() {
    fprintf(stderr, "ERROR: invalid response");
}

void print_connection_closed() {
    fprintf(stderr, "ERROR: connection closed");
}

void print_no_connection() {
    fprintf(stderr, "%s\n", NO_CONNECTION);
	write(out_fd, NO_CONNECTION, strlen(NO_CONNECTION));
}