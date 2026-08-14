#ifndef PARSER_H
#define PARSER_H

#include "protocol.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============ Structs and enum ============ */
// n.b Structs are defined here in the .h because the main.c need to see and
// modify them.

typedef struct parsed_input {
	cmd_type_t type;
	char *key;
	size_t key_size;
	char *val;
	size_t val_size;
} parsed_input_t;

/* ============ Functions ============ */

bool parse_binary(char *payload, uint32_t size, parsed_input_t *parsed);

#endif