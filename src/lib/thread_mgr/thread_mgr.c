/*
* Library:		thread_mgr - manage and interface with a set of threads
* File Name:	thread_mgr.c
*/


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include "log_mgr.h"
#include "thread_mgr.h"
#include "hash_table.h"

#define THREAD_NAME_SIZE 	7
#define MAX_SIGNAL				15

/* For the self-pipe to the manager thread */
#define READ_FD 	0
#define WRITE_FD	1

/* for nicely showing the status of a thread */
static const char *THREAD_STR_STATE[] = {
	"Pending", "Running", "Canceled", "Finished",
};

/* signum to void* handler callbacks */
Hash* SignalHandlers = NULL;

/* Enable flags for the thread-lib-specific signal handlers */
static bool HandleSigInt = true;
static bool HandleSigQuit = true;

/* File descriptors for pipe */
static int PipeFD[2];

/* any initialization that is needed exactly once (globally) can be facilitated
 * with this object */
static pthread_once_t InitDone = PTHREAD_ONCE_INIT;

/* this is the main datastore for thread info (state, name, etc...) */
static struct ThreadInfo *Threads[MAX_THREADS] = {0};

/* this mutex is used to lock the entire Threads[] array when being read from
 * or written to. This should be a reentrant lock to allow for a thread to
 * acquire a lock multiple times */
pthread_mutex_t StoreLock;

/* the thread handle will be stored in Thread Local Storage and used as an
 * index in the Threads[] array (relative to each thread). */
pthread_key_t ThreadHandleKey;

////////////////////////////////////////////////////////////////////////////////
// These functions below are intended to be private (for internal library use only)

// intended to be private
static void thread_signal_handler(int signum) {
	// In case we change 'errno' upon write()
	int savedErrno;
	savedErrno = errno;

	// ensure this is only the subset of signals we are interested in (what fits in a short)
	if (signum > MAX_SIGNAL){
		log_event(FATAL, " [THDLIB] Error: signal handler got an unexpected signal: %d", signum);
		exit(THD_ERROR);
	}

	char sig_short = (char) signum&0xFF;
	if (write(PipeFD[WRITE_FD], &sig_short, 1) == -1 && errno != EAGAIN){
		log_event(FATAL, " [THDLIB] Error: signal handler write failed");
		exit(THD_ERROR);
	}
	errno = savedErrno;
}

// intended to be private
static void selfpipe_init() {
	int flags;

	if (pipe(PipeFD)){
		log_event(FATAL, " [THDLIB] Error: could not create self-pipe");
		exit(THD_ERROR);
	}

	flags = fcntl(PipeFD[WRITE_FD], F_GETFL);
	if (flags == -1){
		log_event(FATAL, " [THDLIB] Error: fcntl get failed");
		exit(THD_ERROR);
	}

	/* ensure writes are nonblocking */
	flags |= O_NONBLOCK;
	if (fcntl(PipeFD[WRITE_FD], F_SETFL, flags)){
		log_event(FATAL, " [THDLIB] Error: fcntl set failed");
		exit(THD_ERROR);
	}
}

// intended to be private
static short selfpipe_wait(){
	char value;
	read(PipeFD[READ_FD], &value, 1);
	return (short) value;
}

// intended to be private
static void show_all_threads() {
	ThreadHandles handle;
	printf("Manged Threads:\n");
	pthread_mutex_lock(&StoreLock);
	for(handle=0; handle < MAX_THREADS; handle++){
		if(Threads[handle] != NULL) {
			pthread_mutex_lock(&Threads[handle]->lock);
			printf("    <Thread>(handle:%d name:%s state:%s)\n",
				Threads[handle]->handle,
				Threads[handle]->name,
				THREAD_STR_STATE[Threads[handle]->state]);
			pthread_mutex_unlock(&Threads[handle]->lock);
		}
	}
	pthread_mutex_unlock(&StoreLock);
}

// intended to be private
static void* mgr_thread(void *args) {
	/* this manager thread should never exit since it is acting as a facility
	for handling signals which workloads are not signal-safe.

	from the man page for pthread_detatch():
		Either pthread_join(3) or pthread_detach() should be called for each
		thread that an application creates, so that system resources for the
		thread can be released.  (But note that the resources of all threads
		are freed when the process terminates.) */

	short signum;
	HashNode* hash_obj;
	SignalHandlerCallback* callback;

	while (1) {
		signum = selfpipe_wait();

		if (signum == -1) {
			/* since the pipe has unexpectedly closed, there is no reason to expect
			any more events to be delivered to this mgr thread. */
			log_event(WARNING, " [THDLIB] Pipe Closed: %d", signum);
			return NULL;
		}

		hash_obj = get_hash_item(SignalHandlers, signum);

		if (hash_obj == NULL){
			log_event(FATAL, " [THDLIB] Error: Unexpected signal: %d", signum);
		} else {
			// invoke the signal handler
			callback = (SignalHandlerCallback*) (hash_obj->value);
			(*(callback->func))();
		}
	}
}

static void sigint_handler() {
	/* REQUIREMENT: print out to standard output the thread handle, the thread name, and
	the status of the thread for all threads that are being managed by the
	library. */
	log_event(INFO, " [THDLIB] Signaled to print thread status (SIGINT)");
	show_all_threads();
}

static void sigquit_handler() {
	/* REQUIREMENT: forcibly terminate or cancel all threads, and clear out the local
	thread databases. (NOTE: the discussion forum notes that the local thread
	database does not need to be cleared explicitly) */

	/* Note: there is no need to exit the manager thread since SIGQUIT does not
	necessarily imply that the application will quit. Infact, the user can
	now spin up more threads after the SIGQUIT handler is used. This is
	why the manager thread is left running even when SIGQUIT is called. */
	log_event(INFO, " [THDLIB] Signaled to kill all threads (SIGQUIT)");
	th_kill_all();
}

// intended to be private
static void thread_init(void) {
	/* I wouldn't do this in real life, but I'd like to have the thread names be
	the same thing on each run, but still appear to be random :)*/
	srand(1);

	/* set a reentrant lock for the Threads[] array */
	pthread_mutexattr_t StoreLockAttr;
	pthread_mutexattr_init(&StoreLockAttr);
	pthread_mutexattr_settype(&StoreLockAttr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&StoreLock, &StoreLockAttr);
	pthread_mutexattr_destroy(&StoreLockAttr);

	/* create an entry in TLS for the thread handle */
	pthread_key_create(&ThreadHandleKey, NULL);

	/* REQUIREMENT: Your library also should catch the SIGQUIT signal. Upon receipt
	of this signal, the library should forcibly terminate or cancel all threads, and
	clear out the local thread databases.

	Given that we do not necessarily have control over main() of applications using
	this library it cannot be assumed that non-reentrant calls can be invoked
	from main()... however, write() without string formatting functions (for the
	thread name) is useless when the function needs to be signal-safe/reentrant.
	The next logical course is to make a detached thread that is responsible for
	performing string manipulation over a static global array (which is not safe
	form within a signal handler) */

	/* create a self pipe to handle non-signal-safe functions from the manager
	thread */
	selfpipe_init();

	pthread_t mgr_thread_obj;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&mgr_thread_obj, &attr, mgr_thread, NULL)) {
		log_event(FATAL, " [THDLIB] Error: cannot create manager thread");
		exit(THD_ERROR);
	}

	/* REQUIREMENT: The signal handler should be installed implicitly when the
	th_execute() function is called the first time.*/

	if (HandleSigQuit){
		th_install_signal_handler(SIGQUIT, &sigquit_handler);
	} else {
		log_event(INFO, " [THDLIB] Not installing SIGINT handler.");
	}

	if (HandleSigInt) {
		th_install_signal_handler(SIGINT, &sigint_handler);
	} else {
		log_event(INFO, " [THDLIB] Not installing SIGQUIT handler.");
	}

}

// intended to be private
static void set_thread_name(char *str, size_t size) {
	const char charset[] = "QWERTYUIOPASDFGHJKLZXCVBNM1234567890";
	if (size) {
		--size;
		size_t n;
		for (n = 0; n < size; n++) {
			int key = rand() % (int) (sizeof charset - 1);
			str[n] = charset[key];
		}
		str[size] = '\0';
	}
}

// intended to be private
static int get_next_handle() {
	int idx = -1;
	pthread_mutex_lock(&StoreLock);
	for(idx=0; idx < MAX_THREADS; idx++){
		if(Threads[idx] == NULL) {
			pthread_mutex_unlock(&StoreLock);
			return idx;
		}
	}
	pthread_mutex_unlock(&StoreLock);

	// tough luck, already managing too many threads!
	return THD_ERROR;
}

// intended to be private
static bool th_valid_handle(ThreadHandles th) {
	bool valid = th >= 0 && th < MAX_THREADS;
	if (!valid) {
		log_event(WARNING, " [THDLIB] Error: given invalid thread handle on operation! Canceling operation... (handle:%d)", th);
	}
	return valid;
}

// intended to be private
static void show_thread(char * msg, ThreadHandles th) {
	if (th_valid_handle(th) && Threads[th] != NULL) {
		pthread_mutex_lock(&Threads[th]->lock);
		log_event(INFO, " %s <Thread>(handle:%d name:%s state:%s pthread:%d)",
										msg,
										Threads[th]->handle,
										Threads[th]->name,
										THREAD_STR_STATE[Threads[th]->state],
										Threads[th]->pthread);
		pthread_mutex_unlock(&Threads[th]->lock);
	} else {
		log_event(INFO, " %s (INVALID) <Thread>(handle:%d)", msg, th);
	}
}

// intended to be private
static void* func_decorator(void *args) {
	struct ThreadInfo* thread_info = args;

	/* ignore all signals in a worker thread */
	sigset_t sig_set;
	sigfillset(&sig_set);
	if(pthread_sigmask(SIG_SETMASK, &sig_set, NULL)){
		log_event(WARNING, " [THDLIB] Error: thread could not block signals!");
	}

	/* before acquiring the lock, make the thread cannot be un-cancellable
	so that the lock won't stay locked if the thread is canceled before it
	is unlocked*/
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	pthread_mutex_lock(&StoreLock);

	// store the handle in TLS for use in th_exit() (free on th_exit() as well)
	ThreadHandles *th = malloc(sizeof(ThreadHandles));
	*th = thread_info->handle;
	pthread_setspecific(ThreadHandleKey, th);

	// update the state of the thread
	thread_info->state = RUNNING;

	show_thread("[THDLIB] Created", thread_info->handle);
	pthread_mutex_unlock(&StoreLock);


	if (pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL)){
		log_event(WARNING, " [THDLIB] Error: unable to defer thread cancels!");
	}
	if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL)){
		log_event(WARNING, " [THDLIB] Error: unable to set thread as cancellable!");
	}

	// run the given function
	thread_info->func(NULL);

	// this call should never return
	int exit_status = th_exit();
	log_event(WARNING, " [THDLIB] Error: th_exit() returned a value! (exit:%d)", exit_status);
	return NULL;
}

// intended to be private
static void th_cleanup(ThreadHandles th) {
	/* REQUIREMENT: The thread information in the library shouldn’t be
	changed until another thread ‘waits’ for the thread (using one of the
	‘th_wait’ calls)...

	After the thread terminates, the thread library should purge the stored
	thread information for the argument thread. */
	pthread_mutex_lock(&StoreLock);
	free(Threads[th]);
	Threads[th] = NULL;
	pthread_mutex_unlock(&StoreLock);
}

////////////////////////////////////////////////////////////////////////////////
// These functions below are intended to be public

ThreadHandles th_execute(Funcptrs func) {
	int handle;

	// do initializtion that should be done exactly once (ever)
	pthread_once(&InitDone, thread_init);

	if(func == NULL) {
		log_event(WARNING, " [THDLIB] Error: given NULL function to th_execute()!");
		return THD_ERROR;
	}

	handle = get_next_handle();
	if (handle != THD_ERROR) {
		pthread_mutex_lock(&StoreLock);

		ThreadInfo *thread_info = (struct ThreadInfo*) malloc(sizeof (ThreadInfo));
		if(thread_info == NULL) {
			log_event(WARNING, " [THDLIB] Failed to allocate thread object!");
			pthread_mutex_unlock(&StoreLock);
			return THD_ERROR;
		}

		// set a reentrant lock for operating on this thread
		pthread_mutexattr_t StoreLockAttr;
		pthread_mutexattr_init(&StoreLockAttr);
		pthread_mutexattr_settype(&StoreLockAttr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&thread_info->lock, &StoreLockAttr);
		pthread_mutexattr_destroy(&StoreLockAttr);

		/* REQUIREMENT: The library will create a name for the thread and maintain it
		within the library */
		set_thread_name(thread_info->name, THREAD_NAME_SIZE);
		thread_info->func = func;
		thread_info->handle = handle;
		thread_info->state = PENDING;

		if(pthread_create(&thread_info->pthread, NULL, func_decorator, (void *) thread_info)) {
			log_event(WARNING, " [THDLIB] Failed to create thread!");
			free(thread_info);

			/* REQUIREMENT:  If the function fails, it shall return THD_ERROR (-1) */
			return THD_ERROR;
		}
		// thread creation was successful!
		Threads[handle] = thread_info;

		pthread_mutex_unlock(&StoreLock);
	}

	/* REQUIREMENT: The library shall also create a unique integer handle
	(ThreadHandles) and return it upon successful execution */
	return handle;
}

int th_wait(ThreadHandles th) {
	if(th_valid_handle(th) && Threads[th] != NULL) {
		pthread_mutex_lock(&Threads[th]->lock);
		pthread_t pthread = Threads[th]->pthread;
		int state = Threads[th]->state;
		pthread_mutex_unlock(&Threads[th]->lock);

		switch (state) {
			case PENDING:
			case RUNNING:
				/* ensure you are not joining while locking the mutex (otherwise other
				threads won't be able to use sensitive funtions from this lib concurrently) */
				show_thread("[THDLIB] Waiting on...", th);
				pthread_join(pthread, NULL);
				show_thread("[THDLIB] ...Wait complete!", th);

				break;
			case CANCELLED:
				/* though a cancel request has occured, the real pthread may still be
				running if it has not reached a cancellation point yet. If the main
				application wants to wait on a cancelled thread that may never exit,
				that is OK! This means that the thread resources should be returned
				to the OS upon user application of th_wait:

				"Note that this call is not required to asynchronously kill the thread;
				the thread may be cancelled until the thread reaches its cancellation
				point, ****and cleaned up after the application waits for the thread.**** " */
				pthread_join(pthread, NULL);
				show_thread("[THDLIB] Reaped (from cancel)", th);
				break;
			case FINISHED:
				/* there is no need to wait on a thread that has already exited with th_exit() */
				show_thread("[THDLIB] Reaped (already finished)", th);
				break;
			default:
				log_event(WARNING, " [%d] Error: Unexpected thread state! handle:%d state:%d", th, Threads[th]->state);
				return THD_ERROR;
		}
		/* purge lib store */
		th_cleanup(th);

		return THD_OK;
	}
	return THD_ERROR;
}

int th_wait_all() {
	/* assume there are no threads being managed */
	int ret = THD_ERROR;
	ThreadHandles idx;
	for(idx=0; idx < MAX_THREADS; idx++){
		/* th_wait() returns THD_OK when there is a thread managed, in this way
		you can easily check for unmanaged threads...

		initial state = -1
		-1 & 0 & 0 & -1 & -1 & -1   ... = 0  (return OK upon at least one existing thread)
		-1 & -1 & -1 & -1 & -1 & -1 ... = -1 (return ERROR upon no managed threads)

		REQUIREMENT: This function returns THD_ERROR if the library is not managing
		any threads or upon any other error condition. Otherwise, the function
		returns THD_OK after all threads terminate. */
		ret &= th_wait(idx);
	}
	return ret;
}

int th_kill(ThreadHandles th) {
	/* Note that this call is not required to asynchronously kill the thread; the
	thread may be cancelled until the thread reaches its cancellation point, and
	cleaned up after the application waits for the thread.

	This means that there is no requirement for this function to "clean up" (nullify)
	any Threads[] entry, as this is the responsibility for the user application to
	call th_wait() */

	if(th_valid_handle(th) && Threads[th] != NULL) {
		pthread_mutex_lock(&Threads[th]->lock);

		if (Threads[th]->state == CANCELLED || Threads[th]->state == FINISHED){
			/* This thread is no longer in a running or soon to be running state and
			therefore cannot be killed. This should result in a THD_ERROR since the
			this handle is not valid for the state it is in. */
			show_thread("[THDLIB] Kill failed (already exited)", th);
			pthread_mutex_unlock(&Threads[th]->lock);
			return THD_ERROR;
		}

		/* REQUIREMENT: This function cancels the executing thread associated with
		the argument thread handle... */
		pthread_cancel(Threads[th]->pthread);

		/* REQUIREMENT: ...and updates the status of the thread appropriately. */
		Threads[th]->state = CANCELLED;

		show_thread("[THDLIB] Killed", th);

		pthread_mutex_unlock(&Threads[th]->lock);
		return THD_OK;
	}
	/* REQUIREMENT: This function returns THD_ERROR if the argument is not a valid
	thread handle. */
	return THD_ERROR;
}

int th_kill_all() {
	/* assume there are no threads being managed */
	int ret = THD_ERROR;
	ThreadHandles idx;
	for(idx=0; idx < MAX_THREADS; idx++){
		/* th_kill() returns THD_OK when there is a thread managed, in this way
		you can easily check for unmanaged threads...

		initial state = -1
		-1 & 0 & 0 & -1 & -1 & -1   ... = 0  (return OK upon at least one existing thread)
		-1 & -1 & -1 & -1 & -1 & -1 ... = -1 (return ERROR upon no managed threads)

		REQUIREMENT: This function returns THD_ERROR if the library is not managing
		any threads or upon any other error condition. Otherwise, the function
		returns THD_OK after all threads terminate. */
		ret &= th_kill(idx);
	}
	return ret;
}


/* This function should allow the thread that calls this function to clean
up its information from the library and exit. */
int th_exit() {
	ThreadHandles* th_ptr = (ThreadHandles*) pthread_getspecific(ThreadHandleKey);
	int th = *th_ptr;
	free(th_ptr);

	/* REQUIREMENT: The thread information in the library should not be purged at
	 * this time; however... the internal status of the thread should be updated. */
	pthread_mutex_lock(&Threads[th]->lock);
	Threads[th]->state = FINISHED;
	pthread_mutex_unlock(&Threads[th]->lock);

	/* REQUIREMENT: ...proper status should be logged to the log file... */
	show_thread("[THDLIB] Exiting", th);

	pthread_exit(NULL);

	/* REQUIREMENT: This call does not return if executed successfully; thus the
	 * only possible return value for this function is THD_ERROR */
	return THD_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
// These functions below are intended for testing purposes only

char* get_thread_name() {
	/* fetches the name of the current thread from TLS, returns NULL otherwise */
	ThreadHandles* th = (ThreadHandles*) pthread_getspecific(ThreadHandleKey);
	if(th_valid_handle(*th) && Threads[*th] != NULL) {
		pthread_mutex_lock(&StoreLock);
		char *name = Threads[*th]->name;
		pthread_mutex_unlock(&StoreLock);
		return name;
	}
	return NULL;
}

const char* get_thread_state(ThreadHandles th) {
	if (th_valid_handle(th) && Threads[th] != NULL) {
		pthread_mutex_lock(&Threads[th]->lock);
		const int state = Threads[th]->state;
		pthread_mutex_unlock(&Threads[th]->lock);
		return THREAD_STR_STATE[state];
	}
	return NULL;
}

void th_use_sigint_handler(bool value) {
	HandleSigInt = value;
}

void th_use_sigquit_handler(bool value) {
	HandleSigQuit = value;
}


bool th_install_signal_handler(int signum, void* handler) {
	sigset_t sigset;
	struct sigaction sa;

	if (signum > MAX_SIGNAL){
		log_event(WARNING, " [THDLIB] Error: Cannot handle signals > 15 (given %d)", signum);
		return false;
	}

	/* the main thread may or may not be blocking signals, we should attempt to
	unblock the ones of interest to this library */
	if (sigemptyset(&sigset) || sigaddset(&sigset, signum)){
		log_event(WARNING, " [THDLIB] Error: Failed to initialize the signal mask (%d)", signum);
		return false;
	}
	if (sigprocmask(SIG_UNBLOCK, &sigset, NULL)){
		log_event(WARNING, " [THDLIB] Error: Failed to unblock %d signal", signum);
		return false;
	}

	// block all other signals while handling a signal
	sigfillset(&sa.sa_mask);

	/* Note: there is a generic handler that is used, not the one passed in (which
	will be invoked from the manager thread) */
	sa.sa_handler = thread_signal_handler;
	sa.sa_flags = SA_RESTART;

	if (sigaction(signum, &sa, NULL)) {
		log_event(FATAL, " [THDLIB] Error: cannot install generic handler for signal (%d)", signum);
		return false;
	}

	// keep the handler for later invocation
	SignalHandlerCallback* callback = malloc(sizeof(SignalHandlerCallback));
	callback->func = handler;

	if (SignalHandlers == NULL) {
		SignalHandlers = new_hash(MAX_SIGNAL);
	}

	insert_hash_item(SignalHandlers, signum, callback, sizeof(void*));

	return true;
}

bool th_uninstall_signal_handler(int signum) {
	sigset_t sigset;

	if (signum > MAX_SIGNAL){
		log_event(WARNING, " [THDLIB] Error: Cannot handle signals > 15 (given %d)", signum);
		return false;
	}

	/* the main thread may or may not be blocking signals, we should attempt to
	block the indicated signal */
	if (sigemptyset(&sigset) || sigaddset(&sigset, signum)){
		log_event(WARNING, " [THDLIB] Error: Failed to initialize the signal mask (%d)", signum);
		return false;
	}
	if (sigprocmask(SIG_BLOCK, &sigset, NULL)){
		log_event(WARNING, " [THDLIB] Error: Failed to block %d signal", signum);
		return false;
	}

	if (SignalHandlers == NULL) {
		log_event(WARNING, " [THDLIB] Error: no signals tracked yet");
		return false;
	}

	delete_hash_item(SignalHandlers, signum);

	return true;
}
