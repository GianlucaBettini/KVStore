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

/* It takes a string and tokenizes it, populating the parsed_input_t struct.
 * Return true on success. */
bool parse_input(char *str, parsed_input_t *parsed);

/* It takes the parsed_input_t struct and executes the command.
 * Return true on success. */
bool exec_cmd(parsed_input_t *parsed, hash_table_t *ht);
