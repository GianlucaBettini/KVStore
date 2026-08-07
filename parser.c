#include "parser.h"
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>

bool parse_binary(char *payload, uint32_t payload_size,
				  parsed_input_t *parsed) {
	uint8_t cmd;
	uint16_t key_size, val_size;
	bool is_set_cmd;
	size_t already_read = 0;

	// TODO: check for payload size
	if (already_read > payload_size)
		return false;

	// === COMMAND
	if (already_read + CMD_HEADER > payload_size)
		return false;
	cmd = (uint8_t)payload[0];
	if (cmd == CMD_INVALID)
		return false;
	is_set_cmd = (cmd == CMD_SET);

	parsed->type = cmd;
	already_read += CMD_HEADER;

	// === KEY
	if (already_read + KEY_HEADER > payload_size)
		return false;

	uint16_t net_key_size;
	memcpy(&net_key_size, payload + already_read, sizeof(uint16_t));
	key_size = ntohs(net_key_size);
	already_read += KEY_HEADER;
	// TODO: check for key_size
	if (already_read + key_size > payload_size)
		return false;

	parsed->key = payload + already_read;
	parsed->key_size = key_size;
	already_read += key_size;

	// === VALUE
	if (is_set_cmd) {
		if (already_read + VAL_HEADER > payload_size)
			return false;

		uint16_t net_val_size;
		memcpy(&net_val_size, payload + already_read, sizeof(uint16_t));
		val_size = ntohs(net_val_size);
		already_read += VAL_HEADER;
		// TODO: check for val_size
		if (already_read + val_size > payload_size)
			return false;

		parsed->val = payload + already_read;
		parsed->val_size = val_size;
		already_read += val_size;
	}

	if (already_read > payload_size)
		return false;

	return true;
}