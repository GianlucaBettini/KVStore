#include "hash_table.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  hash_table_t *ht = ht_create(10); // 10 buckets

  ht_set(ht, "user:1", "mario");
  ht_set(ht, "user:2", "luigi");
  ht_set(ht, "user:1", "wario"); // Overwrites mario

  printf("user:1 is %s\n", ht_get(ht, "user:1")); // Must print wario
  printf("user:2 is %s\n", ht_get(ht, "user:2")); // Must print luigi

  ht_del(ht, "user:1");
  if (ht_get(ht, "user:1") == NULL) {
    printf("user:1 deleted successfully!\n");
  }

  // create the parsed_input struct and initialize it.
  parsed_input_t parsed;
  parsed.key = NULL;
  parsed.val = NULL;

  char str[] = "SET user:1 mario";
  parse_input(str, &parsed);
  exec_cmd(&parsed, ht);

  char str2[] = "GET user:3 mario";
  parse_input(str2, &parsed);
  exec_cmd(&parsed, ht);

  char str3[] = "GET user:2";
  parse_input(str3, &parsed);
  exec_cmd(&parsed, ht);

  char str4[] = "DEL user:2";
  parse_input(str4, &parsed);
  exec_cmd(&parsed, ht);

  char str5[] = "GET user:2";
  parse_input(str5, &parsed);
  exec_cmd(&parsed, ht);

  ht_destroy(ht);
  return 0;
}