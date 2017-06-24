/*
 * Description:
 *   Provide support data structures to sub-projects, specifically a
 *   linked-list implementation.
 *
 * List* new_list()
 * 	create a new list object and return a reference to it. The stored values
 * 	are of any type/size.
 *
 * void push_list_item(List* list, void *value, size_t value_size)
 * 	add a new node with the given value (of size value_size) to the given list.
 *
 * bool remove_list_item(List* list, void* value)
 * 	attempts to remove the given value from the linked list. An indication of
 * 	success is returned.
 *
 * void iterate_list(List *list, void (*processor)(void *))
 * 	iterates across the given list and invokes the given function. This function
 * 	is expected to only take one argument of a type void* which should be cast
 * 	to the value stored in the ListNode->value.
 *
 * void destroy_list(List* list)
 * 	attempt to free the list object and surrounding objects. This does not attempt
 * 	to free the ListNode values contained within the list.
 */


typedef struct ListNode {
	void  *value;
	struct ListNode *next;
} ListNode;

typedef struct List {
	int size;
	ListNode *head;
	ListNode *tail;
} List;


List* new_list();
void push_list_item(List* list, void *value, unsigned int value_size);
bool remove_list_item(List* list, void* value);
void iterate_list(List *list, void (*processor)(void *));
void destroy_list(List* list);
