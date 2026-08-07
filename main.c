#include "client.h"
#include "hash_table.h"
#include "network.h"
#include "parser.h"
#include <asm-generic/errno.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdint.h>
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
#define MAX_CLIENTS                                                            \
	10024 // the real max number of clients is MAX_CLIENTS - 5 because fd 0,1,2
		  // are stdin,stdout,stderr, fd 3,4 are listensock, epollfd
#define MAX_VAL_LEN 1023
#define STATUS_LEN 1
#define STATUS_SUCCESS 0
#define STATUS_ERROR 1
#define STATUS_NOT_FOUND 2
#define STATUS_BAD_REQUEST 3

volatile sig_atomic_t server_running = 1;

client_state_t clients[MAX_CLIENTS];

bool exec_cmd(parsed_input_t *, hash_table_t *, int, int *);

/* Signal handler function: called when SIGINT or SIGTERM are catched (by the
 * kernel). */
void sig_handler(int sig) {
	(void)sig; // don't need it
	server_running = 0;
}

void init_all_clients() {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		clients[i].fd = i;
		clients[i].is_active = false;
		clients[i].read_buf = NULL;
		clients[i].write_buf = NULL;
		clients[i].read_len = 0;
		clients[i].write_len = 0;
		clients[i].read_size = 0;
		clients[i].write_size = 0;
		clients[i].payload_size = 0;
		clients[i].buf_state =
			READING_HEADER; // TODO: not sure about this initialization
	}
}

void init_client(int fd) {
	clients[fd].is_active = true;
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
	clients[fd].payload_size = 0;
	clients[fd].buf_state = READING_HEADER;
}

void free_all_clients() {
	for (int fd = 0; fd < MAX_CLIENTS; fd++) {
		if (clients[fd].write_buf != NULL) {
			free(clients[fd].write_buf);
		}

		if (clients[fd].read_buf != NULL) {
			free(clients[fd].read_buf);
		}

		if (clients[fd].is_active) {
			close(fd);
		}
	}
}

/* Disconnect a client means to reset to 0 the length of the write and read
 * buffers related to the fd of the disconnecting client and closed is set to 1.
 * The write and read buffers are not freed. */
bool disconnect_client(int fd, int *closed) {
	clients[fd].read_len = 0;  // not needed, just defensive programming
	clients[fd].write_len = 0; // same here
	clients[fd].is_active = false;
	close(fd);
	*closed = 1;
	return false;
}

char *get_payload_if_ready(char *read_buf, size_t buf_len,
						   uint32_t *payload_size, buf_state_t *state) {
	if (*state == READING_HEADER) {
		if (buf_len < HEADER_LEN)
			return NULL;

		*payload_size = ntohl(*(uint32_t *)read_buf);
		*state = READING_PAYLOAD;
	}

	if (*state == READING_PAYLOAD) {
		if (buf_len < HEADER_LEN + *payload_size)
			return NULL;

		return read_buf + HEADER_LEN;
	}

	return NULL;
}

/* Append @str to the write buf of the client fd.
 * If there is not enough space, realloc the buffer doubling its size.
 * If the maximum buffer size is reached and surpassed, the client is
 * disconnected.
 * Return true on success, false on disconnection. */
bool buf_append(int fd, const void *data, size_t len, int *closed) {
	while (clients[fd].write_len + len > clients[fd].write_size) {
		if (clients[fd].write_size >= MAX_BUF_SIZE) {
			return disconnect_client(fd, closed);
		}
		clients[fd].write_buf =
			realloc(clients[fd].write_buf, 2 * clients[fd].write_size);
		clients[fd].write_size *= 2;
	}

	memcpy(clients[fd].write_buf + clients[fd].write_len, data, len);
	clients[fd].write_len += len;

	return true;
}

/* Packet = HEADER + PAYLOAD
 * PAYLOAD = [1 byte: status] + [n bytes: data] with n >= 0
 * status: 0 -> success; 1 -> generic error; 2 -> not found */
bool create_outgoing_packet_and_append(int fd, uint32_t payload_size,
									   const void *data, uint8_t status,
									   int *closed) {
	// === HEADER
	uint32_t header = htonl(payload_size);
	if (!buf_append(fd, &header, HEADER_LEN, closed)) {
		return false;
	}

	// === STATUS
	if (!buf_append(fd, &status, STATUS_LEN, closed)) {
		return false;
	}

	// === DATA
	if (data != NULL) {
		if (!buf_append(fd, data, payload_size - STATUS_LEN, closed)) {
			return false;
		}
	}

	return true;
}

/* Initialize the server:
 * network
 * hash table
 * clients states
 * epoll architecture */
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

/* Fill the client's read buffer, placing into it the received bytes, until one
 * of the following: everything is placed correctly into the read buffer
 * (EAGAIN, kernel buf is empty) client disconnected maximum buffer size
 * exceeded. If needed, resize the read buffer. Return true on success, false on
 * disconnection. */
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

/* Drain the client's write buffer sending the bytes in it until one of the
 * following: everything is sent partial sent (kernel buf of the client full).
 * If needed, shift the remaining bytes to sent at the beginning of the client's
 * write buf.
 * If in EPOLLIN branch and partial sent, set EPOLLOUT flag in the
 * entry related to the client of the interest list.
 * If every byte is sent and EPOLLOUT branch, turn off the EPOLLOUT flag.
 * Return true on success, false on disconnection.
 * */
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
	parsed_input_t parsed;
	struct sigaction act = {0};
	char *payload;
	int valid;

	sigemptyset(&act.sa_mask);

	act.sa_handler = sig_handler;
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);

	if (!init_server(&listen_sock, &ht, &epollfd)) {
		return 1;
	}

	while (server_running) {
		nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if (nfds == -1) {
			if (errno == EINTR) {
				continue;
			} else {
				perror("epoll_wait");
				exit(EXIT_FAILURE);
			}
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

					if (conn_sock >= MAX_CLIENTS) {
						printf("Warning: MAX_CLIENTS (%d - 5) reached. "
							   "Rejecting connection...",
							   MAX_CLIENTS);
						close(conn_sock);
						continue;
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

				while (1) {
					payload = get_payload_if_ready(
						clients[evfd].read_buf, clients[evfd].read_len,
						&clients[evfd].payload_size, &clients[evfd].buf_state);

					if (payload == NULL) {
						break;
					}

					valid = 1;
					if (!parse_binary(payload, clients[evfd].payload_size,
									  &parsed)) {
						// TODO: I need a way to create a memory block
						// containing the data and a way to count the bytes
						// without using strlen()
						create_outgoing_packet_and_append(
							evfd, STATUS_LEN, NULL, STATUS_BAD_REQUEST,
							&closed);

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

					memmove(clients[evfd].read_buf,
							clients[evfd].read_buf + HEADER_LEN +
								clients[evfd].payload_size,
							clients[evfd].read_len - HEADER_LEN -
								clients[evfd].payload_size);

					clients[evfd].read_len -=
						(HEADER_LEN + clients[evfd].payload_size);

					clients[evfd].buf_state = READING_HEADER;
					clients[evfd].payload_size = 0;
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

	free_all_clients();
	close(listen_sock);
	ht_destroy(ht);

	return 0;
}

bool exec_cmd(parsed_input_t *parsed, hash_table_t *ht, int fd, int *closed) {
	char *val = NULL;
	size_t val_len;

	switch (parsed->type) {
	case CMD_SET:
		if (ht_set(ht, parsed->key, parsed->val, parsed->key_size,
				   parsed->val_size)) {
			if (!create_outgoing_packet_and_append(fd, STATUS_LEN, NULL,
												   STATUS_SUCCESS, closed))
				return false;
		} else {
			if (!create_outgoing_packet_and_append(fd, STATUS_LEN, NULL,
												   STATUS_ERROR, closed))
				return false;
		}
		break;

	case CMD_GET:
		val = ht_get(ht, parsed->key, parsed->key_size, &val_len);
		if (val == NULL) {
			if (!create_outgoing_packet_and_append(fd, STATUS_LEN, NULL,
												   STATUS_NOT_FOUND, closed))
				return false;
		} else {
			if (!create_outgoing_packet_and_append(fd, STATUS_LEN + val_len,
												   val, STATUS_SUCCESS, closed))
				return false;
		}
		break;

	case CMD_DEL:
		if (ht_del(ht, parsed->key, parsed->key_size)) {
			if (!create_outgoing_packet_and_append(fd, STATUS_LEN, NULL,
												   STATUS_SUCCESS, closed))
				return false;
		} else {
			if (!create_outgoing_packet_and_append(fd, STATUS_LEN, NULL,
												   STATUS_ERROR, closed))
				return false;
		}
		break;

	default:
		return false;
	}

	return true;
}