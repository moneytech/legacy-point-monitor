/*
* Description:
*   Provide the function prototypes for log mangement
*
* int th_execute ( )
*   - This library call executes the argument function as an independent thread
*   within the process. The library will create a name for the thread and maintain
*   it within the library. The library shall also create a unique integer handle
*   (ThreadHandles) and return it upon successful execution. If the function fails,
*   it shall return THD_ERROR (-1).
*
* int th_wait (ThreadHandles)
*   - This call blocks the calling thread until the thread associated with the
*   argument handle terminates.
*   - This call returns THD_ERROR if the argument is not a valid thread. Otherwise
*   the thread returns THD_OK (0).
*   - After the thread terminates, the thread library should purge the stored thread
*   information for the argument thread.
*
* int th_wait_all (void)
*   - This function blocks until all threads in the library terminate.
*   - This function returns THD_ERROR if the library is not managing any threads or
*   upon any other error condition. Otherwise, the function returns THD_OK after all
*   threads terminate.
*   - The thread library should purge the stored thread information for all threads
*   upon successful execution of this call.
*
* int th_kill (ThreadHandles)
*   - This function cancels the executing thread associated with the argument thread
*   handle, and updates the status of the thread appropriately.
*   - This function returns THD_ERROR if the argument is not a valid thread handle.
*   - Note that this call is not required to asynchronously kill the thread; the
*   thread may be cancelled until the thread reaches its cancellation point, and
*   cleaned up after the application waits for the thread.
*
*
* int th_kill_all (void)
*   - This function cancels all threads in the library.
*   - This function returns THD_ERROR if the library is not managing any threads.
*   Otherwise, the function returns THD_OK after all threads are cancelled.
*
* int th_exit (void)
*   - This function should allow the thread that calls this function to clean up its
*   information from the library and exit.
*   - The thread information in the library should not be purged at this time;
*   however, proper status should be logged to the log file, and the internal status
*   of the thread should be updated.
*   - The thread information in the library shouldn’t be changed until another
*   thread ‘waits’ for the thread (using one of the ‘th_wait’ calls).
*   - This call does not return if executed successfully; thus the only possible
*   return value for this function is THD_ERROR.
*
* char* get_thread_name (void)
*   Fetches the name of the current thread from TLS, returns NULL otherwise.
*
* void th_use_sigint_handler (bool)
*   Enables/Disabled installation of the SIGINT handler. Note, this is only valid
*   before the first invocation of th_execute().
*
* void th_use_sigquit_handler (bool)
*   Enables/Disabled installation of the SIGQUIT handler. Note, this is only valid
*   before the first invocation of th_execute().
*
* bool th_install_signal_handler(int signum, void* handler)
*   Install the given signal handler for a signal <= 15. This will be called
*   from the internal manager thread.
*/


#define MAX_THREADS 50
#define THD_OK    	0
#define THD_ERROR 	-1

typedef int ThreadHandles;
typedef void *Funcptrs (void *);

typedef enum {PENDING,		// thread info has been allocated but the thread has not been craeted yet
							RUNNING, 		// thread is positively executing
							CANCELLED,	// pthread_cancel was used to stop the thread prematurely
							FINISHED		// the thread executed to completion (was not canceled and is exited)
} ThreadState;

typedef struct SignalHandlerCallback {
    void (*func)();
} SignalHandlerCallback;

typedef struct ThreadInfo {
	pthread_t pthread;
	pthread_mutex_t lock;
	ThreadHandles handle;
	ThreadState state;
	char name[10];
	void* (*func)(void*);
} ThreadInfo;

ThreadHandles th_execute (Funcptrs);
int th_wait (ThreadHandles);
int th_wait_all (void);
int th_kill (ThreadHandles);
int th_kill_all (void);
int th_exit (void);

// additional functions that are used for testing and logging purposes only
// (this means that they *can* be used improperly, and this should be expected)
char* get_thread_name();
const char* get_thread_state(ThreadHandles);
void th_use_sigint_handler(bool);
void th_use_sigquit_handler(bool);
bool th_install_signal_handler(int signum, void* handler);
bool th_uninstall_signal_handler(int signum);
