#include "client.h"
#include "command.h"
#include "config.h"
#include "hash_table.h"
#include "network.h"
#include "parser.h"
#include "protocol.h"
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

volatile sig_atomic_t server_running = 1;

/* Signal handler function: called when SIGINT or SIGTERM are catched (by the
 * kernel). */
void sig_handler(int sig) {
	(void)sig; // don't need it
	server_running = 0;
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
	size_t processed;

	sigemptyset(&act.sa_mask);

	act.sa_handler = sig_handler;
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);

	if ((rv = create_and_bind_listen_socket(NULL, PORT, &listen_sock)) ==
		ANET_ERR)
		return 1;

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

				processed = 0;
				while (1) {
					payload = get_payload_if_ready(
						clients[evfd].read_buf + processed,
						clients[evfd].read_len - processed,
						&clients[evfd].payload_size, &clients[evfd].buf_state);

					if (payload == NULL) {
						break;
					}

					processed += HEADER_LEN + clients[evfd].payload_size;

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

					clients[evfd].buf_state = READING_HEADER;
					clients[evfd].payload_size = 0;
				}

				if (closed)
					continue;

				if (processed > 0) {
					memmove(clients[evfd].read_buf,
							clients[evfd].read_buf + processed,
							clients[evfd].read_len - processed);

					clients[evfd].read_len -= processed;
				}

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
	close(epollfd);
	ht_destroy(ht);

	return 0;
}
