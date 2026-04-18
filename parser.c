#include "parser.h"
#include <ctype.h>
#include <string.h>

/* Convert @str to lowercase. */
void tolower_string(char *str) {
  if (str == NULL)
    return;

  for (int i = 0; str[i] != 0; i++) {
    str[i] = (char)tolower((unsigned char)str[i]);
  }
}

/* Assign the right command enum to the token. */
cmd_type_t parse_cmd(char *token) {
  // Convert to lowercase to make command parsing case-insensitive
  tolower_string(token);

  if (strcmp(token, "get") == 0)
    return CMD_GET;
  else if (strcmp(token, "set") == 0)
    return CMD_SET;
  else if (strcmp(token, "del") == 0)
    return CMD_DEL;
  else
    return CMD_INVALID;
}

bool parse_input(char *str, parsed_input_t *parsed) {
  cmd_type_t cmd_type;
  char *state_ptr; // used in strtok_r to save the state in the string

  char *token = strtok_r(str, " \r\n", &state_ptr);
  if (token == NULL)
    return false;
  cmd_type = parse_cmd(token);
  if (cmd_type == CMD_INVALID)
    return false;
  parsed->type = cmd_type;

  token = strtok_r(NULL, " \r\n", &state_ptr);
  if (token == NULL)
    return false;
  parsed->key = token;

  // TODO: Currently I ignore the extra arguments (tokens) (e.g. GET key extra1)
  // In the future it must validate the exact number of args for each command.
  switch (parsed->type) {
  case CMD_SET:
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