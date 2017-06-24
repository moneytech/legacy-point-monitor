/*
* Library: shared_mem - a library to utilize and manage shared memory
*/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "log_mgr.h"
#include "hash_table.h"
#include "list.h"
#include "shared_mem.h"

/* key to SegmentNode lookup */
Hash* SegmentNodes = NULL;

/* controls whether or not semaphores are created on connect_shm() */
bool UseSemaphores = false;


void use_semaphores(bool set){
	UseSemaphores = set;

	if (UseSemaphores)
		log_event(WARNING, " [LIBSHM] Enabling semaphore usage.");
	else
		log_event(WARNING, " [LIBSHM] Disabling semaphore usage.");
}


bool shm_lock(int key) {
	HashNode *segment_hash_obj;
	SegmentNode *node;
	struct sembuf sem;

	if (UseSemaphores) {
		/* obtain the shared segment id from the global data structure */
		segment_hash_obj = get_hash_item(SegmentNodes, key);
		if (segment_hash_obj == NULL){
			log_event(WARNING, " [LIBSHM] Error: Unable to find node to lock (key:%d)", key);
			return false;
		}
		node = segment_hash_obj->value;

		/* wait on the semaphore (unless it's value is non-negative) */
		sem.sem_num = 0;
		sem.sem_op = -1;
		sem.sem_flg = SEM_UNDO;
		if (semop(node->lock_id, &sem, 1) == SHM_ERROR){
			log_event(WARNING, " [LIBSHM] Error: Unable to lock segment (key:%d)", key);

			/* this should be fatal since it probably indicates that the semaphore
			is gone, thus we should no longer operate on this shared memory segment */
			return false;
		}
	}

	return true;
}


bool shm_unlock(int key) {
	HashNode *segment_hash_obj;
	SegmentNode *node;
	struct sembuf sem;

	if (UseSemaphores) {
		/* obtain the shared segment id from the global data structure */
		segment_hash_obj = get_hash_item(SegmentNodes, key);
		if (segment_hash_obj == NULL){
			log_event(WARNING, " [LIBSHM] Error: Unable to find node to unlock (key:%d)", key);
			return false;
		}
		node = segment_hash_obj->value;

		/* signal the semaphore (increase its value by one) */
		sem.sem_num = 0;
		sem.sem_op = 1;
		sem.sem_flg = SEM_UNDO;

		if (semop(node->lock_id, &sem, 1) == SHM_ERROR){
			//log_event(WARNING, " [LIBSHM] Error: Unable to unlock segment (key:%d)", key);

			/* this should be fatal since it probably indicates that the semaphore
			is gone, thus we should no longer operate on this shared memory segment */
			return false;
		}
	}
	return true;
}


// intended to be private
static void show_segment_node(void *hash_node){
	int attachments;
	SegmentNode* node = ((HashNode*) hash_node)->value;
	attachments = ((SegmentNode *)node)->attachments->size;

	log_event(WARNING, " ● Segment(key=%d, shm_id=%d, size=%d, attachments=%d)",
						((SegmentNode *)node)->key,
						((SegmentNode *)node)->shm_id,
						((SegmentNode *)node)->size,
						attachments);

	/* I chose not to use iterate_list to make formatting of the list to look nicer */
	ListNode *list_node = ((SegmentNode *)node)->attachments->head;
	while (list_node != NULL) {
		attachments -= 1;
		if (attachments == 0) {
			log_event(WARNING, "   └── Attachment(addr=%p)", list_node->value);
		} else {
			log_event(WARNING, "   ├── Attachment(addr=%p)", list_node->value);
		}
		list_node = list_node->next;
	}
}


void show_segments() {
	if (SegmentNodes != NULL && SegmentNodes->size > 0) {
		iterate_hash(SegmentNodes, show_segment_node);
	} else {
		log_event(WARNING, " No segments created yet");
	}
}


void* connect_shm(int key, int size) {
	int shm_id;
	void* shm_ptr;
	struct sembuf sem;
	HashNode* segment_hash_obj;
	SegmentNode* node;

	if (SegmentNodes == NULL) {

		/* REQ_conn_3: A program using this library function must be able to use it to
		attach the maximum number of shared memory segments to the calling process.
		(Note that Solaris 11 does not have a limit to the number of attachments, so you
		can use the limit that Linux supports. */
		SegmentNodes = new_hash(SHM_MAX_SEGMENTS);
	}

	if ((shm_id = shmget(key, size, IPC_CREAT | 0644)) == -1) {
		log_event(WARNING, " [LIBSHM] Error: Unable to get shared memory segment (%d): %s", errno, strerror(errno));
		return NULL;
	}


	/* REQ_conn_3: A program using this library function must be able to use it to
	attach the maximum number of shared memory segments to the calling process.
	(Note that Solaris 11 does not have a limit to the number of attachments, so you
	can use the limit that Linux supports.

	Note: internally attachments are tracked via a linked list, which allows as
	many nodes as there is memory. The max number of attachments on linux is 65514.
	This lib will continue to attempt to attach to segments until it can't (shmat
	returns -1), so this is allowing the most number of attachments to occur
	regardless of the system. */

	shm_ptr = shmat(shm_id, NULL, 0);
	if ((intptr_t)shm_ptr == -1) {
		log_event(WARNING, " [LIBSHM] Error: Unable to attach to shared memory segment (%d): %s", errno, strerror(errno));

		/* REQ_conn_2 If, for some reason, this function cannot connect to the shared
		memory area as requested, it shall return a NULL pointer. */
		return NULL;
	}

	segment_hash_obj = get_hash_item(SegmentNodes, key);
	if (segment_hash_obj == NULL){
		// this is the first time we've seen this segment, take note of it
		node = (SegmentNode*) malloc(sizeof(SegmentNode));
		node->key = key;
		node->size = size;
		node->shm_id = shm_id;
		node->attachments = (List*) new_list();

		if (UseSemaphores) {
			/* create the semaphore (which will be locked by default)*/
			if ((node->lock_id = semget(key, 1, IPC_CREAT | 0644)) == SHM_ERROR) {
				log_event(WARNING, " [LIBSHM] Error: Unable to create a lock for the given memory segment");

				/* since all coordinated operations depend on the use of a semaphore, not
				being able to get a semephore should be 'fatal' */
				return NULL;
			}

			/* unlock the semaphore (locked by default when created) */
			sem.sem_num = 0;
			sem.sem_op = 1;
			sem.sem_flg = SEM_UNDO;
			if (semop(node->lock_id, &sem, 1) == SHM_ERROR){
				log_event(WARNING, " [LIBSHM] Error: Unable to unlock (key:%d)", key);
			}

		} else {
			node->lock_id = SHM_ERROR;
		}

		/* semaphore and shared memory segment obtained! */
		insert_hash_item(SegmentNodes, key, node, sizeof(SegmentNode));

	} else {
		// this is a new attachment to a segment key that was already used
		node = segment_hash_obj->value;
	}

	// add the attachment address to the segment node list
	push_list_item(node->attachments, (void*) shm_ptr, sizeof(shm_ptr));

	/* REQ_conn_1: The return value for this function is a pointer to the shared
	memory area which has been attached (and possibly created) by this function.

	Note: from the discussion board: on multiple invocations of this function
	this should return multiple attachment addresses. */
	return shm_ptr;
}


// this is intended to be private
static int find_key_for_address(Hash* nodes, void* addr) {
	int i;
	SegmentNode* node;
	ListNode* attachment_list_obj;

	// for all segments...
	for(i = 0; i<nodes->size; i++) {
		if(nodes->hash_array[i] != NULL){
			node = nodes->hash_array[i]->value;

			// for all attachments to a segment...
			attachment_list_obj = node->attachments->head;
			while (attachment_list_obj != NULL) {

				// find the matching address (from what was given)
				if ( attachment_list_obj->value == addr){
					return node->key;
				}
				attachment_list_obj = attachment_list_obj->next;
			}
		}
	}
	return SHM_ERROR;
}


static void destroy_shm_lock(int key) {
	HashNode *segment_hash_obj;
	SegmentNode *node;
	struct sembuf sem;
	struct shmid_ds ds_obj;

	if (UseSemaphores) {
		segment_hash_obj = get_hash_item(SegmentNodes, key);
		if (segment_hash_obj == NULL){
			log_event(WARNING, " [LIBSHM] Error: Unexpected key given to destroy_shm_lock (key:%d)", key);
			return;
		}

		node = segment_hash_obj->value;

		if (shmctl(node->shm_id, IPC_STAT, &ds_obj) == -1) {
			/* Invalid Argument: when a bad id is given (say one that has already been destroyed) */
			if (errno == 22) {
				ds_obj.shm_nattch = 0;
			} else {
				log_event(WARNING, " [LIBSHM] Error: Unable to get segment stats (key:%d, shm_id:%d): %d %s", key, node->shm_id, errno, strerror(errno));
				return;
			}
		}

		/* stats acquired, only remove lock if the shared segment is not being used by anyone */
		if (ds_obj.shm_nattch != 0) {
			log_event(INFO, " [LIBSHM] Segment is still in use (nattach:%d). The lock will not be destroyed. (key:%d)", ds_obj.shm_nattch, key);
			return;
		}

		sem.sem_num = 0;
		sem.sem_op = 0;
		sem.sem_flg = SEM_UNDO;

		if (semctl(node->lock_id, 0, IPC_RMID, sem) == SHM_ERROR) {
			/* Invalid Argument: when a bad id is given (say one that has already been destroyed) */
			if (errno != 22) {
				log_event(FATAL, " [LIBSHM] Error: Unable to destroy lock (key:%d): %d %s", key, errno, strerror(errno));
			}
			return;
		}
		log_event(INFO, " [LIBSHM] Segment lock destroyed (key:%d)", key);
	}
}


int detach_shm(void* addr) {
	HashNode* segment_hash_obj;
	SegmentNode* node;
	int key;

	key = find_key_for_address(SegmentNodes, addr);

	if (key == SHM_ERROR){
		log_event(WARNING, " [LIBSHM] Error: Address does not belong to an attached shared memory segment! (addr:%p)", addr );

		/* REQ_detach_2: This function will return OK (0) on success, and ERROR (-1) otherwise. */
		return SHM_ERROR;
	}

	/* REQ_detach_1: This function detaches the shared memory segment attached to the process via the argument addr. */
	if (shmdt(addr) == SHM_ERROR) {
		log_event(WARNING, " [LIBSHM] Error: Could not detatch shared memory segment (addr:%p): %s (%d)",  addr, strerror(errno), errno );

		/* REQ_detach_2: This function will return OK (0) on success, and ERROR (-1) otherwise. */
		return SHM_ERROR;
	}

	// remove address from attachment for this segment
	segment_hash_obj = get_hash_item(SegmentNodes, key);
	if (segment_hash_obj == NULL){
		log_event(WARNING, " [LIBSHM] Error: Expected to find Segment Obj, but not found (addr:%p, key:%d)", addr, key);
	} else {
		node = segment_hash_obj->value;
		if (remove_list_item(node->attachments, addr) == false) {
			log_event(WARNING, " [LIBSHM] Error: Expected to find Address in Segment Obj attachment list, but not found (addr:%p, key:%d)", addr, key);
		}
	}

	/* destroy semephore (if no other attachments on the memory segment are detected)
	Note: if semaphore usage is disabled this will do nothing */
	destroy_shm_lock(key);

	/* REQ_detach_2: This function will return OK (0) on success, and ERROR (-1) otherwise. */
	return SHM_OK;
}


int destroy_shm(int key) {
	HashNode *segment_hash_obj;
	SegmentNode *node;
	ListNode *cur, *next;

	/* REQ_destroy_1: This function detaches all shared memory segments (attached to
	the calling process by connect_shm( )) associated with the argument key from the
	calling process. */
	segment_hash_obj = get_hash_item(SegmentNodes, key);
	if (segment_hash_obj == NULL){
		log_event(WARNING, " [LIBSHM] Error: Unexpected key given to destroy_shm (key:%d)",key);

		/* REQ_destroy_3: This function will return OK (0) on success, and ERROR (-1) otherwise. */
		return SHM_ERROR;
	}

	// perform the detach for each address found...
	node = segment_hash_obj->value;
	cur = node->attachments->head;
	while (cur != NULL) {
		next = cur->next;
		/* no need to check this return value since we need to iterate accross this
		entire list and logging of errors is facilitated by detach_shm() */
		detach_shm(cur->value);
		cur = next;
	}

	/* destroy semephore (if no other attachments on the memory segment are detected)
	Note: if semaphore usage is disabled this will do nothing */
	destroy_shm_lock(key);

	/* REQ_destroy_2: The shared memory segment is then subsequently deleted from the system.*/
	if(shmctl(node->shm_id, IPC_RMID, 0) != 0) {
		if (errno == 22){
			/* though this "worked", we entered this function expecting a segment to be there and to
			be deletable, which was not the case. Thus an error is still returned. */
			log_event(WARNING, " [LIBSHM] Segment has (probably) already been destroyed (key:%d)", key);

			/* remove the metadata from the lib store since it is already positively gone */
			delete_hash_item(SegmentNodes, key);
		} else {
			log_event(FATAL, " [LIBSHM] Error: Unable to destroy shared memory segment (key:%d): %s (%d)", key, strerror(errno), errno);
		}
		return SHM_ERROR;
	} else {
		log_event(INFO, " [LIBSHM] Segment flagged to be destroyed (key:%d)", key);
	}

	/* remove the metadata from the lib store if successfully (positively) removed */
	delete_hash_item(SegmentNodes, key);

	/* REQ_destroy_3: This function will return OK (0) on success, and ERROR (-1) otherwise. */
	return SHM_OK;
}
