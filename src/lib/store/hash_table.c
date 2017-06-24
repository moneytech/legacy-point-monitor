/*
 * Library: store - a generic set of storage data structures
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "hash_table.h"


Hash* new_hash(int size) {
	int i;
	Hash *hash = (Hash*)malloc(sizeof(Hash)+size*sizeof(HashNode));
	hash->size = size;
	for(i = 0; i<size; i++) {
		hash->hash_array[i] = NULL;
	}
	return hash;
}

static int get_hash_index(Hash* hash, int key) {
	return ((unsigned int)key) % hash->size;
}


HashNode *get_hash_item(Hash* hash, int key) {
	 int hash_index = get_hash_index(hash, key);

	while(hash->hash_array[hash_index] != NULL) {
		if(hash->hash_array[hash_index]->key == key){
			return hash->hash_array[hash_index];
		}

		++hash_index;
		hash_index %= hash->size;
	}

	return NULL;
}

void insert_hash_item(Hash* hash, int key, void* value, int size) {
	HashNode *item = (HashNode*) malloc(sizeof(HashNode));
	item->size = size;
	item->value = value;
	item->key = key;

	int hash_index = get_hash_index(hash, key);

	while(hash->hash_array[hash_index] != NULL) {
		// allow for overwrites
		if (hash->hash_array[hash_index]->key == key){
			break;
		}
		++hash_index;
		hash_index %= hash->size;
	}
	hash->hash_array[hash_index] = item;
}

bool delete_hash_item(Hash* hash, int key) {
	int hash_index = get_hash_index(hash, key);

	while(hash->hash_array[hash_index] != NULL) {
		if(hash->hash_array[hash_index]->key == key) {
			free(hash->hash_array[hash_index]);
			hash->hash_array[hash_index] = NULL;
			return true;
		}

		++hash_index;
		hash_index %= hash->size;
	}

	return false;
}

void iterate_hash(Hash *hash, void (*processor)(void *)){
	int i = 0;
	for(i = 0; i<hash->size; i++) {
		if(hash->hash_array[i] != NULL) {
			(*processor)(hash->hash_array[i]);
		}
	}

}

void show_hash(void* hash_node) {
	printf("HashNode(key=%d, value=%p)\n", ((HashNode*) hash_node)->key,
																				((HashNode*) hash_node)->value);
}


void destroy_hash(Hash* hash) {
	int i;
	for(i = 0; i<hash->size; i++) {
		if(hash->hash_array[i] != NULL){
			free(hash->hash_array[i]);
		}
	}
	free(hash);
}
