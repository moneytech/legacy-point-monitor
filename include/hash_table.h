/*
 * Description:
 *   Provide support data structures to sub-projects, specifically
 *   a simple hash table implementation. This supports only integer keys but
 *   any type/size value.
 *
 * Hash* new_hash(int size)
 * 	allocates a new hash table up to the given number of nodes (size)
 *
 * HashNode *get_hash_item(Hash* hash, int key)
 * 	given a hash and potential key, this function returns a HashNode object
 * 	that contains the key and value. If this key is not in the hash then NULL
 * 	is returned.
 *
 * void insert_hash_item(Hash* hash, int key, void* value, int size)
 * 	inserts a new HashNode object with the given key and value into the given hash.
 *
 * bool delete_hash_item(Hash* hash, int key)
 * 	attempts to delete the key/value pair from the given hash. An indication of
 * 	success is returned as a boolean.
 *
 * void iterate_hash(Hash *hash, void (*processor)(void *))
 * 	iterates across the given hash and invokes the given function. This function
 * 	is expected to only take one argument of a type void* which should be cast
 * 	to the value stored in the HashNode->value.
 *
 * void show_hash(void* hash_node)
 * 	dump a representation of the given hash node to the log
 *
 * void destroy_hash(Hash* hash)
 * 	attempt to free the hash object and surrounding objects. This does not attempt
 * 	to free the HashNode values contained within the hash.
 */


typedef struct HashNode {
	int key;
	int size;
	void* value;
} HashNode;

typedef struct Hash {
	int size;
	HashNode* hash_array[];
} Hash;

Hash* new_hash(int size);

HashNode *get_hash_item(Hash* hash, int key) ;
void insert_hash_item(Hash* hash, int key, void* value, int size);
bool delete_hash_item(Hash* hash, int key);

void iterate_hash(Hash *hash, void (*processor)(void *));
void show_hash(void* hash_node);

void destroy_hash(Hash* hash);
