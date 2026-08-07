#include "hash_table.h"
#include <stdbool.h>
#include <stdint.h>

// Maybe they are better in a new section dedicated to the protocol
#define HEADER_LEN 4
#define CMD_HEADER 1
#define KEY_HEADER 2
#define VAL_HEADER 2

/* ============ Structs and enum ============ */
// n.b Structs are defined here in the .h because the main.c need to see and
// modify them.

typedef enum command_type { CMD_SET, CMD_GET, CMD_DEL, CMD_INVALID } cmd_type_t;

typedef struct parsed_input {
	cmd_type_t type;
	char *key;
	size_t key_size;
	char *val;
	size_t val_size;
} parsed_input_t;

/* ============ Functions ============ */

/* Parses a raw command string, populating the parsed_input_t struct.
 * WARNING: This function uses zero-copy parsing, so it mutates the input @str
 * by replacing delimiters with '\0'. The caller must ensure @str is mutable and
 * must not free @parsed->key and @parsed->val. Return true on success. */
bool parse_input(char *str, parsed_input_t *parsed);

bool parse_binary(char *payload, uint32_t size, parsed_input_t *parsed);
