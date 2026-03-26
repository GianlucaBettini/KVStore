#include <stdbool.h>
#include <stddef.h> // for size_t

/* =============== Opaque pointers =============== */

typedef struct kv_node kv_node_t;

typedef struct hash_table hash_table_t;

/* ============= Prototypes ============= */

/* 1. Lifecycles */

/* Creates the hash table. */
hash_table_t *ht_create(size_t num_buckets);

/* Frees the hash table. */
void ht_destroy(hash_table_t *ht);

/* 2. Core operations */

/* Insert a new entry (key, val) if the key is not already present in the table.
 * Replace the val if the key is already present.
 * Return true on success.
 */
bool ht_set(hash_table_t *ht, const char *key, const char *val);

/* Delete the entry identified by the key. Return true if found and deleted. */
bool ht_del(hash_table_t *ht, const char *key);

/* Returns a pointer to the value string or NULL if not found. */
char *ht_get(hash_table_t *ht, const char *key);

/* 3. Utility functions */

/* The hash function to turn a key into an integer. */
size_t hash_function(const char *key, size_t num_bucket);