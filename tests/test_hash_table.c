#include "../hash_table.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  hash_table_t *ht = ht_create(10);
  assert(ht != NULL);
  assert(ht_set(ht, "key1", "one") == true);
  assert(strcmp(ht_get(ht, "key1"), "one") == 0);
  ht_set(ht, "key1", "new_val_one");
  assert(strcmp(ht_get(ht, "key1"), "new_val_one") == 0);
  assert(ht_del(ht, "key1") == true);
  assert(ht_get(ht, "key1") == NULL);
  ht_destroy(ht);
  printf("All unit tests passed!\n");

  return 0;
}