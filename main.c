#include "hash_table.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>

#define BUF_SIZE 256

int main(void) {
  hash_table_t *ht = ht_create(10);

  parsed_input_t parsed;
  parsed.key = NULL;
  parsed.val = NULL;

  // REPL implementation.

  char buf[BUF_SIZE];

  while (1) {
    printf("kv> ");
    fflush(stdout);

    if (!fgets(buf, BUF_SIZE, stdin)) {
      printf("End of file. Exit...\n");
      break;
    }

    if (buf[0] == '\n')
      continue;

    if (!parse_input(buf, &parsed)) {
      printf("Invalid syntax\n");
      continue;
    }

    exec_cmd(&parsed, ht);
  }

  ht_destroy(ht);
  return 0;
}