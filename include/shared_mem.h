/*
* Description:
*   Provide the function prototypes for managing shared memory
*
* void* connect_shm(int key, int size)
* 	- REQ_conn_1: This function has two arguments:
* 	  - The first argument serves as the key for the shared memory segment.
* 	  - The second argument contains the size (in bytes) of the shared memory segment to be allocated.
* 	- REQ_conn_2: The return value for this function is a pointer to the shared memory area which has been attached (and possibly created) by this function.
* 	- REQ_conn_3: If, for some reason, this function cannot connect to the shared memory area as requested, it shall return a NULL pointer.
* 	- REQ_conn_4: A program using this library function must be able to use it to attach the maximum number of shared memory segments to the calling process. (Note that Solaris 11 does not have a limit to the number of attachments, so you can use the limit that Linux supports).
*
* int detach_shm(void *addr)
* 	- REQ_detach_1: This function detaches the shared memory segment attached to the process via the argument addr.
* 	- REQ_detach_2: The associated shared memory segment is not deleted from the system.
* 	- REQ_detach_3: This function will return OK (0) on success, and ERROR (-1) otherwise.
*
* int destroy_shm(int key)
* 	- REQ_destroy_1: This function detaches all shared memory segments (attached to the calling process by connect_shm( )) associated with the argument key from the calling process.
* 	- REQ_destroy_2: The shared memory segment is then subsequently deleted from the system.
* 	- REQ_destroy_3: This function will return OK (0) on success, and ERROR (-1) otherwise.
*
* void show_segments()
*		loops accross all shared memory segments currently connected and logs them
*
* bool shm_lock(int key)
*		Note: this function does nothing unless use_semaphores(true) is called.
*		Given the same key used for the _shm* functions, this function attempts to
*		lock the systemV semaphore (created upon connect_shm). This is useful when
*		attempting to coordinate shared memory access accross processes. Return
*		true if the lock was positively acquired, returns false otherwise.
*
* bool shm_unlock(int key)
*		Note: this function does nothing unless use_semaphores(true) is called.
*		Given the same key used for the _shm* functions, this function attempts to
*		unlock the systemV semaphore (created upon connect_shm). This is useful when
*		attempting to coordinate shared memory access accross processes. Return
*		true if the lock was positively unlocked, returns false otherwise.
*
* void use_semaphores(bool set)
*		Setting to true enables the use of sem_lock() and sum_unlock() for coordinating
*		access to a shared memory segment. By default semaphores are not created and
*		shm_lock() and shm_unlock() do nothing until 'true' is passed into an invocation
*		of this function. This must be called before using connect_shm or else undefined
*		behavior will occur.
*
* Note on semaphore behavior: semaphores are created on connect_shm() and destroyed
* on destroy_shm() and detach_shm() only if the detected number of attachments
* for the segment being protected by the semaphore is 0. This is not fool-proof
* since multiple apps can be using effectively different segments but using
* the same semaphore, thus the attachment count can be wrong which could lead to
* unsupported behavior (deleting the semaphore while another process is still
* using it).
*/


#define SHM_OK                     0
#define SHM_ERROR                  -1
#define SHM_MAX_SEGMENTS           4096
#define SHM_MAX_LINUX_ATTACHMENTS  65514

typedef struct SegmentNode {
	int key;
	int shm_id;
	int lock_id;
	int size;
	List* attachments;
} SegmentNode;

void* connect_shm(int key, int size);
int detach_shm(void* addr);
int destroy_shm(int key);
void show_segments();

bool shm_lock(int key);
bool shm_unlock(int key);

void use_semaphores(bool set);
