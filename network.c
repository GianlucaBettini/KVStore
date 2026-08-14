#include "network.h"
#include "config.h"
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
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

int create_and_bind_listen_socket(char *host, char *port, int *listen_sock) {
	int rv;

	if ((rv = net_info(host, port, listen_sock)) == ANET_ERR) {
		return ANET_ERR;
	}

	if ((rv = net_listen(*listen_sock, BACKLOG)) == ANET_ERR) {
		close(*listen_sock);
		return ANET_ERR;
	}

	printf("Server listening on port %s\n", port);

	return ANET_OK;
}
