/*
 * Library: store - a generic set of storage data structures
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "list.h"

List* new_list() {
	List *list = (List*)malloc(sizeof(List));
	list->size = 0;
	list->head = NULL;
	list->tail = NULL;
	return list;
}

void push_list_item(List* list, void* value, unsigned int value_size) {
	int idx;
	ListNode* new_node = (ListNode*)malloc(sizeof(ListNode)+value_size);
	new_node->value = value;
  new_node->next = NULL;

	for (idx=0; idx<value_size; idx++) {
		*(char *)(new_node->value + idx) = *(char *)(value + idx);
	}

	if (list->head == NULL) {
		list->head = new_node;
	} else {
		list->tail->next = new_node;
	}

	list->tail = new_node;
	list->size += 1;
}

bool remove_list_item(List* list, void* value) {
	ListNode *prev = NULL;
	ListNode *node = list->head;
	while (node != NULL) {
		if (node->value == value) {

			// if this is head, replace it
			if (prev == NULL){
				list->head = node->next;
			} else {
				prev->next = node->next;
			}

			// if this is tail, replace it
			if (prev == NULL){
				list->tail = NULL;
			} else {
				list->tail = prev;
			}

			list->size -= 1;
			return true;
		}
		prev = node;
		node = node->next;
	}
	return false;
}

void iterate_list(List *list, void (*processor)(void *)){
	ListNode *node = list->head;
	while (node != NULL) {
		(*processor)(node->value);
		node = node->next;
	}
}

void destroy_list(List* list) {
  ListNode *last;
	ListNode *node = list->head;
	while (node != NULL) {
		last = node;
		node = node->next;
		free(last);
	}
	free(list);
}
