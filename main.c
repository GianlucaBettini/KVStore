#include "hash_table.h"
#include <stdio.h>

int main(void) {
  hash_table_t *ht = ht_create(10); // 10 buckets

  ht_set(ht, "user:1", "mario");
  ht_set(ht, "user:2", "luigi");
  ht_set(ht, "user:1", "wario"); // Overwrites mario

  printf("user:1 is %s\n", ht_get(ht, "user:1")); // Must print mario
  printf("user:2 is %s\n", ht_get(ht, "user:2")); // Must print luigi

  ht_del(ht, "user:1");
  if (ht_get(ht, "user:1") == NULL) {
    printf("user:1 deleted successfully!\n");
  }

  ht_destroy(ht);
  return 0;
}