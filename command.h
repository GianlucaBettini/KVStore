#ifndef COMMAND_H
#define COMMAND_H

#include "hash_table.h"
#include "parser.h"
#include <stdbool.h>

bool exec_cmd(parsed_input_t *parsed, hash_table_t *ht, int fd, int *closed);

#endif