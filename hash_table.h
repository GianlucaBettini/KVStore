#include <stdbool.h>
#include <stddef.h> // for size_t

/* =============== Opaque pointers =============== */

typedef struct kv_node kv_node_t;

typedef struct hash_table hash_table_t;

/* ============= Prototypes ============= */

/* === Lifecycles */

/* Create the hash table. */
hash_table_t *ht_create(size_t num_buckets);

/* Free the hash table. */
void ht_destroy(hash_table_t *ht);

/* === Core operations */

/* If the key is not already present, allocate a new node and insert it
 * into the ht.
 * Otherwise, update the value related to the key.
 * Before creating a new node, the load factor has to be checked and the hash
 * table resized, if needed.
 * Return true on success.
 */
bool ht_set(hash_table_t *ht, const char *key, const char *val, size_t key_len,
			size_t val_size);

/* Delete the entry identified by the key. Return true on success. */
bool ht_del(hash_table_t *ht, const char *key, size_t key_len);

/* Returns a pointer to the value string or NULL if not found. */
char *ht_get(hash_table_t *ht, const char *key, size_t key_len,
			 size_t *out_val_len);

/* === Utility functions */

/* Hash function to turn a key into an integer. */
size_t hash_function(const char *key, size_t key_size, size_t num_bucket);