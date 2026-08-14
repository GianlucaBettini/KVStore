#ifndef CLIENT_H
#define CLIENT_H

#include "config.h"
#include "protocol.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
	int fd;
	bool is_active;
	char *read_buf, *write_buf;
	size_t read_len, write_len;
	size_t read_size, write_size;
	buf_state_t buf_state;
	uint32_t payload_size;
} client_state_t;

extern client_state_t clients[MAX_CLIENTS];

void init_all_clients();

void init_client(int fd);

void free_all_clients();

/* Disconnect a client means to reset to 0 the length of the write and read
 * buffers related to the fd of the disconnecting client and closed is set to 1.
 * The write and read buffers are not freed. */
bool disconnect_client(int fd, int *closed);

char *get_payload_if_ready(char *read_buf, size_t buf_len,
						   uint32_t *payload_size, buf_state_t *state);

/* Append @str to the write buf of the client fd.
 * If there is not enough space, realloc the buffer doubling its size.
 * If the maximum buffer size is reached and surpassed, the client is
 * disconnected.
 * Return true on success, false on disconnection. */
bool buf_append(int fd, const void *data, size_t len, int *closed);

/* Packet = HEADER + PAYLOAD
 * PAYLOAD = [1 byte: status] + [n bytes: data] with n >= 0
 * status: 0 -> success; 1 -> generic error; 2 -> not found */
bool create_outgoing_packet_and_append(int fd, uint32_t payload_size,
									   const void *data, uint8_t status,
									   int *closed);

/* Fill the client's read buffer, placing into it the received bytes, until one
 * of the following: everything is placed correctly into the read buffer
 * (EAGAIN, kernel buf is empty) client disconnected maximum buffer size
 * exceeded. If needed, resize the read buffer. Return true on success, false on
 * disconnection. */
bool fill_client_read_buf(int fd, int *closed);

/* Drain the client's write buffer sending the bytes in it until one of the
 * following: everything is sent partial sent (kernel buf of the client full).
 * If needed, shift the remaining bytes to sent at the beginning of the client's
 * write buf.
 * If in EPOLLIN branch and partial sent, set EPOLLOUT flag in the
 * entry related to the client of the interest list.
 * If every byte is sent and EPOLLOUT branch, turn off the EPOLLOUT flag.
 * Return true on success, false on disconnection.
 * */
bool drain_client_write_buf(int fd, int epollfd, int *closed, int is_epollout);

#endif