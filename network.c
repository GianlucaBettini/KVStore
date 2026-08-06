#include "network.h"
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int net_info(char *host, char *port, int *sockfd) {
	struct addrinfo hints, *info, *p;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(host, port, &hints, &info)) != 0)
		return ANET_ERR;

	for (p = info; p != NULL; p = p->ai_next) {
		if ((rv = net_socket(p, sockfd)) == -1) // or != 0 ???
			continue;
		fcntl(*sockfd, F_SETFL, O_NONBLOCK);

		if ((rv = net_setsockopt(*sockfd)) == -1) {
			close(*sockfd);
			return ANET_ERR;
		}

		if ((rv = net_bind(*sockfd, p)) == -1) {
			close(*sockfd);
			continue;
		}

		break;
	}

	freeaddrinfo(info);

	if (p == NULL)
		return ANET_ERR;

	return ANET_OK;
}

int net_socket(struct addrinfo *p, int *sockfd) {
	if ((*sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		return ANET_ERR;

	return ANET_OK;
}

int net_setsockopt(int sockfd) {
	int yes = 1;

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		return ANET_ERR;

	return ANET_OK;
}

int net_bind(int sockfd, struct addrinfo *p) {
	if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		return ANET_ERR;

	return ANET_OK;
}

int net_listen(int sockfd, int backlog) {
	if (listen(sockfd, backlog) == -1)
		return ANET_ERR;

	return ANET_OK;
}

int net_accept(int sockfd, int *clientfd,
			   struct sockaddr_storage *client_addr) {
	socklen_t sin_size = sizeof *client_addr;

	if ((*clientfd =
			 accept(sockfd, (struct sockaddr *)client_addr, &sin_size)) == -1)
		return ANET_ERR;

	return ANET_OK;
}

ssize_t net_recv(int clientfd, char *buf, size_t len) {
	int num_bytes = recv(clientfd, buf, len, 0);

	return num_bytes;
}

ssize_t net_send(int clientfd, char *buf, size_t len) {
	int nbytes = send(clientfd, buf, len, 0);
	return nbytes;
}

/* TODO: move it elsewhere */
int get_command_to_scan(char *server_buf, size_t *curr_buf_len,
						char *buf_to_parse, char target) {
	size_t command_len;

	char *newline_ptr = memchr(server_buf, target, *curr_buf_len);
	if (newline_ptr == NULL)
		return ANET_ERR;

	command_len = newline_ptr - server_buf + 1; // +1 because I count also '\n'

	memcpy(buf_to_parse, server_buf, command_len - 1);
	buf_to_parse[command_len - 1] = 0;

	memmove(server_buf, server_buf + command_len, *curr_buf_len - command_len);
	*curr_buf_len -= command_len;

	return ANET_OK;
}
