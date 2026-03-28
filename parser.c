#include "parser.h"
#include "hash_table.h"
#include <stdio.h>
#include <string.h>

/* Assign the right command enum to the token. */
cmd_type_t parse_cmd(char *token) {
  if (strcmp(token, "GET") == 0)
    return CMD_GET;
  else if (strcmp(token, "SET") == 0)
    return CMD_SET;
  else if (strcmp(token, "DEL") == 0)
    return CMD_DEL;
  else
    return CMD_INVALID;
}

bool parse_input(char *str, parsed_input_t *parsed) {
  cmd_type_t cmd_type;
  char *state_ptr; // used in strtok_r to save the state in the string

  // parse the first token, which is the command. If invalid, return false (on
  // error).
  char *token = strtok_r(str, " \r\n", &state_ptr);
  if (token == NULL)
    return false;
  cmd_type = parse_cmd(token);
  if (cmd_type == CMD_INVALID)
    return false;
  parsed->type = cmd_type;

  // parse the second token, so the key. If NULL, return false (on error).
  token = strtok_r(NULL, " \r\n", &state_ptr);
  if (token == NULL)
    return false;
  parsed->key = token;

  // switch on the eventual third token.
  switch (parsed->type) {
  case CMD_SET:
    // parse the third token, which is the val.
    token = strtok_r(NULL, " \r\n", &state_ptr);
    if (token == NULL) {
      return false;
    }
    parsed->val = token;
    break;

  case CMD_DEL:
  case CMD_GET:
    break;
  default:
    return false;
  }

  return true;
}

bool exec_cmd(parsed_input_t *parsed, hash_table_t *ht) {
  char *val = NULL;
  switch (parsed->type) {
  case CMD_SET:
    if (ht_set(ht, parsed->key, parsed->val))
      printf("OK\n");
    else
      printf("Not OK\n");
    break;
  case CMD_GET:
    val = ht_get(ht, parsed->key);
    if (val == NULL)
      printf("Not found\n");
    else
      printf("%s\n", val);
    break;
  case CMD_DEL:
    if (ht_del(ht, parsed->key)) {
      printf("Deleted\n");
    } else {
      printf("Not found\n");
    }
    break;
  default:
    return false;
  }

  return true;
}