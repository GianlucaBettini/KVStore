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
#include <unistd.h>

#define MAX_BUF_SIZE 32768		// 2^15
#define INIT_READ_BUF_SIZE 8192 // 2^13
#define INIT_WRITE_BUF_SIZE 8192
#define PORT "8080"
#define BACKLOG 10
#define NUM_BUCKETS 10000
#define MAX_EVENTS 128
#define MAX_CLIENTS 10024

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

int main(void) {
	hash_table_t *ht;
	int rv;
	int listen_sock, conn_sock;
	struct sockaddr_storage client_addr;
	int epollfd, nfds;
	struct epoll_event ev, events[MAX_EVENTS];
	ssize_t nbytes;
	char buf_to_parse[MAX_BUF_SIZE];
	parsed_input_t parsed;

	if ((rv = net_info(NULL, PORT, &listen_sock)) == ANET_ERR) {
		return 1;
	}

	if ((rv = net_listen(listen_sock, BACKLOG)) == ANET_ERR) {
		close(listen_sock);
		return 1;
	}

	printf("Server listening on port %s\n", PORT);

	ht = ht_create(NUM_BUCKETS);
	init_all_clients();

	epollfd = epoll_create1(0);
	if (epollfd == -1) {
		perror("epoll_create1");
		exit(EXIT_FAILURE);
	}

	ev.events = EPOLLIN;
	ev.data.fd = listen_sock;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
		perror("epoll_ctl: listen_sock");
		exit(EXIT_FAILURE);
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
				while (1) {
					if (clients[evfd].read_len >= MAX_BUF_SIZE) {
						// TODO error: reached max buffer size
						close(evfd);
						closed = 1;
						break;
					} else if (clients[evfd].read_len ==
							   clients[evfd].read_size) {
						clients[evfd].read_buf =
							realloc(clients[evfd].read_buf,
									2 * clients[evfd].read_size);
						clients[evfd].read_size *= 2;
					}

					nbytes = net_recv(
						evfd, clients[evfd].read_buf + clients[evfd].read_len,
						clients[evfd].read_size - clients[evfd].read_len);

					if (nbytes == 0) {
						// client disconnected
						// TODO i have to manage the disconnection of the client
						// (the close(clientfd))
						close(evfd);
						closed = 1;
						break;
					} else if (nbytes == -1) {
						if (errno == EAGAIN || errno == EWOULDBLOCK) {
							break;
						} else {
							// error
							close(evfd);
							closed = 1;
							break;
						}
					}

					clients[evfd].read_len += nbytes;
				}

				if (closed == 1)
					continue;

				while ((rv = get_command_to_scan(
							clients[evfd].read_buf, &clients[evfd].read_len,
							buf_to_parse, '\n')) == ANET_OK) {
					int valid = 1;
					if (!parse_input(buf_to_parse, &parsed)) {
						while (clients[evfd].write_len + 15 + 1 >
							   clients[evfd].write_size) {
							if (clients[evfd].write_size >= MAX_BUF_SIZE) {
								// error
								close(evfd);
								closed = 1;
								break;
							}
							clients[evfd].write_buf =
								realloc(clients[evfd].write_buf,
										2 * clients[evfd].write_size);
							clients[evfd].write_size *= 2;
						}
						if (!closed) {
							strcpy(clients[evfd].write_buf +
									   clients[evfd].write_len,
								   "Invalid syntax\n");
							clients[evfd].write_len += 15;
							valid = 0;
						}
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

				nbytes = net_send(evfd, clients[evfd].write_buf,
								  clients[evfd].write_len);

				if (nbytes == -1) {
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
						ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
						ev.data.fd = evfd;
						epoll_ctl(epollfd, EPOLL_CTL_MOD, evfd, &ev);
					} else {
						// TODO error
						close(evfd);
						closed = 1;
						break;
					}

				} else if (nbytes >= 0) {
					if ((size_t)nbytes == clients[evfd].write_len) {
						clients[evfd].write_len = 0;
						ev.events = EPOLLIN | EPOLLET;
						ev.data.fd = evfd;
						epoll_ctl(epollfd, EPOLL_CTL_MOD, evfd, &ev);
					} else if ((size_t)nbytes < clients[evfd].write_len) {
						memmove(clients[evfd].write_buf,
								clients[evfd].write_buf + nbytes,
								clients[evfd].write_len - nbytes);
						clients[evfd].write_len -= nbytes;
						ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
						ev.data.fd = evfd;
						epoll_ctl(epollfd, EPOLL_CTL_MOD, evfd, &ev);
					} else {
						// TODO error
						close(evfd);
						closed = 1;
						break;
					}
				}

			} else if (events[i].events & EPOLLOUT) {
				// EPOLLOUT routine S5
				while (1) {
					nbytes = net_send(evfd, clients[evfd].write_buf,
									  clients[evfd].write_len);

					if (nbytes == -1) {
						if (errno == EAGAIN || errno == EWOULDBLOCK) {
							break; // already with EPOLLOUT
						} else {
							// TODO error
							close(evfd);
							closed = 1;
							break;
						}

					} else if (nbytes >= 0) {
						if ((size_t)nbytes == clients[evfd].write_len) {
							clients[evfd].write_len = 0;
							ev.events = EPOLLIN | EPOLLET;
							ev.data.fd = evfd;
							epoll_ctl(epollfd, EPOLL_CTL_MOD, evfd, &ev);
							break;
						} else if ((size_t)nbytes < clients[evfd].write_len) {
							memmove(clients[evfd].write_buf,
									clients[evfd].write_buf + nbytes,
									clients[evfd].write_len - nbytes);
							clients[evfd].write_len -= nbytes;

						} else {
							// TODO error
							close(evfd);
							closed = 1;
							break;
						}
					}
				}
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
			while (clients[fd].write_len + 3 + 1 > clients[fd].write_size) {
				if (clients[fd].write_size >= MAX_BUF_SIZE) {
					// error
					close(fd);
					*closed = 1;
					return false;
				}
				clients[fd].write_buf =
					realloc(clients[fd].write_buf, 2 * clients[fd].write_size);
				clients[fd].write_size *= 2;
			}
			strcpy(clients[fd].write_buf + clients[fd].write_len, "OK\n");
			clients[fd].write_len += 3;
		} else {
			while (clients[fd].write_len + 7 + 1 > clients[fd].write_size) {
				if (clients[fd].write_size >= MAX_BUF_SIZE) {
					// error
					close(fd);
					*closed = 1;
					return false;
				}
				clients[fd].write_buf =
					realloc(clients[fd].write_buf, 2 * clients[fd].write_size);
				clients[fd].write_size *= 2;
			}
			strcpy(clients[fd].write_buf + clients[fd].write_len, "Not OK\n");
			clients[fd].write_len += 7;
		}
		break;

	case CMD_GET:
		val = ht_get(ht, parsed->key);
		if (val == NULL) {
			while (clients[fd].write_len + 10 + 1 > clients[fd].write_size) {
				if (clients[fd].write_size >= MAX_BUF_SIZE) {
					// error
					close(fd);
					*closed = 1;
					return false;
				}
				clients[fd].write_buf =
					realloc(clients[fd].write_buf, 2 * clients[fd].write_size);
				clients[fd].write_size *= 2;
			}
			strcpy(clients[fd].write_buf + clients[fd].write_len,
				   "Not found\n");
			clients[fd].write_len += 10;
		} else {
			size_t val_len = strlen(val);
			while (clients[fd].write_len + val_len + 2 >
				   clients[fd].write_size) {
				if (clients[fd].write_size >= MAX_BUF_SIZE) {
					// error
					close(fd);
					*closed = 1;
					return false;
				}
				clients[fd].write_buf =
					realloc(clients[fd].write_buf, 2 * clients[fd].write_size);
				clients[fd].write_size *= 2;
			}
			snprintf(clients[fd].write_buf + clients[fd].write_len,
					 clients[fd].write_size - clients[fd].write_len, "%s\n",
					 val);
			clients[fd].write_len += val_len + 1;
		}
		break;

	case CMD_DEL:
		if (ht_del(ht, parsed->key)) {
			while (clients[fd].write_len + 8 + 1 > clients[fd].write_size) {
				if (clients[fd].write_size >= MAX_BUF_SIZE) {
					// error
					close(fd);
					*closed = 1;
					return false;
				}
				clients[fd].write_buf =
					realloc(clients[fd].write_buf, 2 * clients[fd].write_size);
				clients[fd].write_size *= 2;
			}
			strcpy(clients[fd].write_buf + clients[fd].write_len, "Deleted\n");
			clients[fd].write_len += 8;
		} else {
			while (clients[fd].write_len + 10 + 1 > clients[fd].write_size) {
				if (clients[fd].write_size >= MAX_BUF_SIZE) {
					// error
					close(fd);
					*closed = 1;
					return false;
				}
				clients[fd].write_buf =
					realloc(clients[fd].write_buf, 2 * clients[fd].write_size);
				clients[fd].write_size *= 2;
			}
			strcpy(clients[fd].write_buf + clients[fd].write_len,
				   "Not found\n");
			clients[fd].write_len += 10;
		}
		break;

	default:
		return false;
	}

	return true;
}