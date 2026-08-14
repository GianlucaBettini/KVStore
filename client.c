#include "client.h"
#include "network.h"
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

client_state_t clients[MAX_CLIENTS];

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