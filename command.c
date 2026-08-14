#include "command.h"
#include "client.h"

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
		create_outgoing_packet_and_append(fd, STATUS_LEN, NULL,
										  STATUS_BAD_REQUEST, closed);
		return false;
	}

	return true;
}