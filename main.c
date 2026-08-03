#include "hash_table.h"
#include "network.h"
#include "parser.h"
#include <asm-generic/errno.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_BUF_SIZE 32768		// 2^15
#define INIT_READ_BUF_SIZE 8192 // 2^13
#define INIT_WRITE_BUF_SIZE 8192
#define PORT "8080"
#define BACKLOG 10
#define NUM_BUCKETS 10000
#define MAX_EVENTS 128
#define MAX_CLIENTS 10024
#define MAX_VAL_LEN 1023

typedef struct {
	int fd;
	char *read_buf, *write_buf;
	size_t read_len, write_len;
	size_t read_size, write_size;
} client_state_t;

client_state_t clients[MAX_CLIENTS];

bool exec_cmd(parsed_input_t *, hash_table_t *, int, int *);

void init_all_clients() {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		clients[i].fd = i;
		clients[i].read_buf = NULL;
		clients[i].write_buf = NULL;
		clients[i].read_len = 0;
		clients[i].write_len = 0;
		clients[i].read_size = 0;
		clients[i].write_size = 0;
	}
}

void init_client(int fd) {
	if (clients[fd].read_buf == NULL) {
		clients[fd].read_buf = malloc(INIT_READ_BUF_SIZE);
		clients[fd].read_size = INIT_READ_BUF_SIZE;
	}
	if (clients[fd].write_buf == NULL) {
		clients[fd].write_buf = malloc(INIT_WRITE_BUF_SIZE);
		clients[fd].write_size = INIT_WRITE_BUF_SIZE;
	}
	clients[fd].read_len = 0;
	clients[fd].write_len = 0;
}

bool disconnect_client(int fd, int *closed) {
	clients[fd].read_len = 0;  // not needed, just defensive programming
	clients[fd].write_len = 0; // same here
	close(fd);
	*closed = 1;
	return false;
}

bool buf_append(int fd, const char *str, int *closed) {
	int len = strlen(str) + 1;
	while (clients[fd].write_len + len > clients[fd].write_size) {
		if (clients[fd].write_size >= MAX_BUF_SIZE) {
			return disconnect_client(fd, closed);
		}
		clients[fd].write_buf =
			realloc(clients[fd].write_buf, 2 * clients[fd].write_size);
		clients[fd].write_size *= 2;
	}

	strcpy(clients[fd].write_buf + clients[fd].write_len, str);
	clients[fd].write_len += (len - 1);

	return true;
}

bool init_server(int *listen_sock, hash_table_t **ht, int *epollfd) {
	int rv;
	struct epoll_event ev;

	if ((rv = net_info(NULL, PORT, listen_sock)) == ANET_ERR) {
		return false;
	}

	if ((rv = net_listen(*listen_sock, BACKLOG)) == ANET_ERR) {
		close(*listen_sock);
		return false;
	}

	printf("Server listening on port %s\n", PORT);

	*ht = ht_create(NUM_BUCKETS);

	init_all_clients();

	*epollfd = epoll_create1(0);
	if (*epollfd == -1) {
		perror("epoll_create1");
		exit(EXIT_FAILURE);
	}

	ev.events = EPOLLIN;
	ev.data.fd = *listen_sock;
	if (epoll_ctl(*epollfd, EPOLL_CTL_ADD, *listen_sock, &ev) == -1) {
		perror("epoll_ctl: listen_sock");
		exit(EXIT_FAILURE);
	}

	return true;
}

bool fill_client_read_buf(int fd, int *closed) {
	ssize_t nbytes;
	while (1) {
		if (clients[fd].read_len >= MAX_BUF_SIZE) {
			return disconnect_client(fd, closed);
		} else if (clients[fd].read_len == clients[fd].read_size) {
			clients[fd].read_buf =
				realloc(clients[fd].read_buf, 2 * clients[fd].read_size);
			clients[fd].read_size *= 2;
		}

		nbytes = net_recv(fd, clients[fd].read_buf + clients[fd].read_len,
						  clients[fd].read_size - clients[fd].read_len);

		if (nbytes == 0) {
			return disconnect_client(fd, closed);
		} else if (nbytes == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				break;
			} else {
				return disconnect_client(fd, closed);
			}
		}

		clients[fd].read_len += nbytes;
	}

	return true;
}

bool drain_client_write_buf(int fd, int epollfd, int *closed, int is_epollout) {
	struct epoll_event ev;
	ssize_t nbytes;

	while (clients[fd].write_len > 0) {
		nbytes = net_send(fd, clients[fd].write_buf, clients[fd].write_len);

		if (nbytes == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (!is_epollout) {
					ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
					ev.data.fd = fd;
					epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
				}
				break;
			} else {
				return disconnect_client(fd, closed);
			}
		}

		if (nbytes == 0) {
			return disconnect_client(fd, closed);
		}

		// nbytes > 0

		if ((size_t)nbytes == clients[fd].write_len) {
			clients[fd].write_len = 0;
		} else if ((size_t)nbytes < clients[fd].write_len) {
			memmove(clients[fd].write_buf, clients[fd].write_buf + nbytes,
					clients[fd].write_len - nbytes);
			clients[fd].write_len -= nbytes;
		} else {
			// not reachable, just defensive programming
			return disconnect_client(fd, closed);
		}
	}

	if (clients[fd].write_len == 0 && is_epollout) {
		ev.events = EPOLLIN | EPOLLET;
		ev.data.fd = fd;
		epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
	}

	return true;
}

int main(void) {
	hash_table_t *ht;
	int rv;
	int listen_sock, conn_sock;
	struct sockaddr_storage client_addr;
	int epollfd, nfds;
	struct epoll_event ev, events[MAX_EVENTS];
	char buf_to_parse[MAX_BUF_SIZE];
	parsed_input_t parsed;

	if (!init_server(&listen_sock, &ht, &epollfd)) {
		return 1;
	}

	while (1) {
		nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if (nfds == -1) {
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}

		for (int i = 0; i < nfds; i++) {
			int evfd = events[i].data.fd;
			int closed = 0;

			if (evfd == listen_sock) {
				// listen sock routine S3
				while (1) {
					// S1
					if ((rv = net_accept(listen_sock, &conn_sock,
										 &client_addr)) == ANET_ERR) {
						// S5
						if (errno == EAGAIN || errno == EWOULDBLOCK) {
							break;
						} else {
							perror("accept");
							exit(EXIT_FAILURE);
						}
					}
					// S2
					fcntl(conn_sock, F_SETFL, O_NONBLOCK);
					// S3
					init_client(conn_sock);

					// S4
					ev.events = EPOLLIN | EPOLLET;
					ev.data.fd = conn_sock;
					if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev) ==
						-1) {
						perror("epoll_ctl: conn_sock");
						exit(EXIT_FAILURE);
					}
				}

			} else if (events[i].events & EPOLLIN) {
				// EPOLLIN routine S4
				if (!fill_client_read_buf(evfd, &closed))
					continue;

				while ((rv = get_command_to_scan(
							clients[evfd].read_buf, &clients[evfd].read_len,
							buf_to_parse, '\n')) == ANET_OK) {
					int valid = 1;
					if (!parse_input(buf_to_parse, &parsed)) {
						buf_append(evfd, "Invalid syntax\n", &closed);
						valid = 0;
					}

					if (closed)
						break;
					if (valid) {
						if (!exec_cmd(&parsed, ht, evfd, &closed)) {
							// TODO error
							if (closed)
								break;
						}
					}
				}

				if (closed)
					continue;

				if (!drain_client_write_buf(evfd, epollfd, &closed, false))
					continue;

			} else if (events[i].events & EPOLLOUT) {
				// EPOLLOUT routine S5
				if (!drain_client_write_buf(evfd, epollfd, &closed, true))
					continue;
			}
		}
	}
	close(listen_sock);
	ht_destroy(ht);

	return 0;
}

bool exec_cmd(parsed_input_t *parsed, hash_table_t *ht, int fd, int *closed) {
	char *val = NULL;

	switch (parsed->type) {
	case CMD_SET:
		if (ht_set(ht, parsed->key, parsed->val)) {
			if (!buf_append(fd, "OK\n", closed))
				return false;
		} else {
			if (!buf_append(fd, "Not OK\n", closed))
				return false;
		}
		break;

	case CMD_GET:
		val = ht_get(ht, parsed->key);
		if (val == NULL) {
			if (!buf_append(fd, "Not found\n", closed))
				return false;
		} else {
			char temp_buf[MAX_VAL_LEN + 2];
			snprintf(temp_buf, sizeof(temp_buf), "%s\n", val);
			if (!buf_append(fd, temp_buf, closed))
				return false;
		}
		break;

	case CMD_DEL:
		if (ht_del(ht, parsed->key)) {
			if (!buf_append(fd, "Deleted\n", closed))
				return false;
		} else {
			if (!buf_append(fd, "Not found\n", closed))
				return false;
		}
		break;

	default:
		return false;
	}

	return true;
}