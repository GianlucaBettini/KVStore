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

// void init_all_clients(client_state_t *clients);
