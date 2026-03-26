#include "hash_table.h"
#include <stdlib.h>
#include <string.h>

/* ========= Structs ========= */

typedef struct kv_node {
  char *key;
  char *value;
  struct kv_node *next;
} kv_node_t;

typedef struct hash_table {
  size_t num_buckets;
  kv_node_t **buckets;
  size_t num_entries;
} hash_table_t;

/* ========= Implementations ========= */

hash_table_t *ht_create(size_t num_buckets) {
  hash_table_t *ht = malloc(sizeof(*ht));
  if (ht == NULL) {
    return NULL;
  }

  ht->num_buckets = num_buckets;
  ht->num_entries = 0;
  ht->buckets = calloc(num_buckets, sizeof(*ht->buckets));
  if (ht->buckets == NULL) {
    free(ht);
    return NULL;
  }
  return ht;
}

void free_node(kv_node_t *node) {
  free(node->key);   // because of strdup()
  free(node->value); // same here
  free(node);
}

void free_bucket(kv_node_t *node) {
  kv_node_t *next = node->next;
  free_node(node);

  if (next != NULL)
    free_bucket(next);
}

void ht_destroy(hash_table_t *ht) {
  for (size_t i = 0; i < ht->num_buckets; i++) {
    kv_node_t *curr_bucket = ht->buckets[i];
    if (curr_bucket == NULL)
      continue;
    free_bucket(curr_bucket);
  }
  free(ht->buckets);
  free(ht);
}

char *ht_get(hash_table_t *ht, const char *key) {
  size_t idx = hash_function(key, ht->num_buckets);
  char *val = NULL;
  kv_node_t *head = ht->buckets[idx];

  while (head) {
    if (strcmp(head->key, key) == 0) {
      val = head->value;
      break;
    } else {
      head = head->next;
    }
  }

  return val;
}

bool ht_del(hash_table_t *ht, const char *key) {
  size_t idx = hash_function(key, ht->num_buckets);
  kv_node_t *head = ht->buckets[idx];

  if (head == NULL)
    return false; // bucket empty. Not found.

  if (strcmp(head->key, key) == 0) { // found at the top of the bucket.
    kv_node_t *node_to_free = head;
    ht->buckets[idx] = head->next;
    free_node(node_to_free);
    return true;
  }

  bool found = false;

  // it may be somewhere in the bucket.
  while (head->next && !found) {
    if (strcmp(head->next->key, key) == 0) {
      found = true;
      kv_node_t *node_to_free = head->next;
      head->next = head->next->next;
      free_node(node_to_free);
    } else {
      head = head->next;
    }
  }

  return found;
}

kv_node_t *create_kv_node(char *key, char *val) {
  kv_node_t *new_node = malloc(sizeof(*new_node));
  if (new_node == NULL) {
    return NULL;
  }

  new_node->key = key;
  new_node->value = val;
  new_node->next = NULL;

  return new_node;
}

/* It still doesn't manage errors.
 * The check of the strdup allocation is not yet implemented. */
bool ht_set(hash_table_t *ht, const char *key, const char *val) {
  size_t idx = hash_function(key, ht->num_buckets);
  kv_node_t *head = ht->buckets[idx];
  bool found = false;

  while (head && !found) {
    if (strcmp(head->key, key) == 0) {
      found = true;
      char *old_val = head->value;
      head->value = strdup(val);
      free(old_val);
    } else {
      head = head->next;
    }
  }

  if (found) // found and value changed.
    return true;
  else { // not found -> add the node to the top
    kv_node_t *new_node = create_kv_node(strdup(key), strdup(val));
    if (new_node == NULL)
      return false; // error
    new_node->next = ht->buckets[idx];
    ht->buckets[idx] = new_node;
    return true;
  }
}

/* It implements, for now, the djb2 by Dan Bernstein. */
size_t hash_function(const char *str, size_t num_bucket) {
  unsigned long hash = 5381;
  int c;

  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c; /* equivalent to: hash * 33 + c */
  }

  return hash % num_bucket;
}