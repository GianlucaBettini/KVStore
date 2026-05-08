#include "hash_table.h"
#include <stdbool.h>

/* ============ Structs and enum ============ */
// n.b Structs are defined here in the .h because the main.c need to see and
// modify them.

typedef enum command_type { CMD_SET, CMD_GET, CMD_DEL, CMD_INVALID } cmd_type_t;

typedef struct parsed_input {
  cmd_type_t type;
  char *key;
  char *val;
} parsed_input_t;

/* ============ Functions ============ */

/* Parses a raw command string, populating the parsed_input_t struct.
 * WARNING: This function uses zero-copy parsing, so it mutates the input @str
 * by replacing delimiters with '\0'. The caller must ensure @str is mutable and
 * must not free @parsed->key and @parsed->val. Return true on success. */
bool parse_input(char *str, parsed_input_t *parsed);
