/*
* Description:
*
* The install_data program takes a file name as an argument. The file will contain
* lines of text which describes the data to be written into shared memory and the
* time (relative to the start time of the program) at which the data shall be
* placed in the shared memory area. The program will perform the following:
*
* - Verify that the argument file is provided on the command line, and that the
*   file can be opened for reading.
*
* - Call connect_shm( ) which should return a pointer to the shared memory area.
*
* - Process the data from the file, a line at a time. Write the data to the shared
*   memory at the designated time. (Be sure to verify that the index given in the
*   input file is valid.) After you have installed all the data according to the
*   input file, don’t put any other data into shared memory.
*
* - Call destroy_shm( ) to delete the shared memory segment from the system. (Does
*   this need to be coordinated with the use of the shared memory by monitor_shm?
*   Why or why not?)
*
* - Exit.
*
* The file which install_data reads will be a text file. Each line of the file will
* follow the following format:
*
*     <index> <x_value> <y_value> <time increment>¬
*
* where:
*
* - index ranges from 0 to 19 and indicates which element of the shared memory
*   structure is to be written to;
* - x_value and y_value are floating point numbers which are to be installed in the
*   x and y members of that structure.
*
* If the time increment variable is nonnegative, then this value represents the
* integral number of seconds to delay until the data on that line are installed in
* the shared memory. If the time increment value is negative, the absolute value
* of this increment represents the integral number of seconds to delay before
* making the corresponding index invalid. (The x and y values are ignored in this
* case.) There can be any number of white space (tabs or spaces) between each
* field on a line.
*
* Additionally, install_data should handle errors in the input data file; the
* output of install_data (that is, what gets installed into shared memory) is
* undefined in this case, but the program should never "core dump" or get caught
* in an infinite loop due to errors in the format of the input file. (For more
* details on the operation of install_data, see the Example below.)
*
* Additionally, upon receipt of a SIGHUP signal, the install_data program should
* clear the shared memory area, and re-install the requested data as described
* above from the beginning of the input file. Also, upon the receipt of a SIGTERM
* or SIGINT signal, the install_data program shall detach and destroy the shared
* memory segment and then exit.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "log_mgr.h"
#include "thread_mgr.h"
#include "hash_table.h"
#include "list.h"
#include "shared_mem.h"
#include "point.h"

#define SHM_KEY   8675309
#define OK        0

/* this represents the work to be done from the input file. A list of tasks
will  represent the file in global memory */
List* Tasks;

/* The shared memory address to install the data will be stored here */
void* ShmAddr;
bool ReinstallTasks = false;

pthread_cond_t TaskingCompleted;
pthread_mutex_t SyncMutex;

static void create_entry(List* task_list, char* line) {
	int found_items;
	PointTask *task;

	task = malloc(sizeof(PointTask));

	found_items = sscanf(line, "%d %f %f %d",
								&task->index,
								&task->point.x,
								&task->point.y,
								&task->delay);
	if (found_items != 4){
		log_event(FATAL, " [MAIN] Unable to parse line (%d items found). Skipping entry. (line:'%s')", found_items, line);
		return;
	}

	task->point.is_valid = 1;

	/* REQ_install_data_3: ...Be sure to verify that the index given in the input file is valid.... */

	/* NOTE_install_data_1: Additionally, install_data should handle errors in the input data file; the
	output of install_data (that is, what gets installed into shared memory) is
	undefined in this case

	Answer: only install valid points defined in the file, otherwise skip over invalid point
	installations/invalidations */
	if (task->index < 0 || task->index >= MAX_NUM_POINTS) {
		log_event(WARNING, " [MAIN] Error: invalid point index given (%d). Skipping entry.", task->index);
	} else {
		push_list_item(task_list, (void *) task, sizeof(PointTask));
	}

}

static void process_entry(void* node) {
	char * name = get_thread_name();
	PointTask *task = (PointTask *) node;

	/* REQ_install_data_5: If the time increment variable is nonnegative, then this
	value represents the integral number of seconds to delay until the data on that
	line are installed in the shared memory. If the time increment value is
	negative, the absolute value of this increment represents the integral number of
	seconds to delay before making the corresponding index invalid. (The x and y
	values are ignored in this case.). */
	log_event(INFO, " [%s] Sleeping %d", name, task->delay);
	sleep(abs(task->delay));

	if (task->index < 0 || task->index >= MAX_NUM_POINTS){
		log_event(WARNING, " [%s] Skipping task due to bad index (%d)", name, task->index);
		return;
	}

	/* REQ_install_data_3: ...Write the data to the shared memory at the designated
	time... */
	if(shm_lock(SHM_KEY)) {
		if (task->delay >= 0){
			install_point(ShmAddr, task->index, &task->point);
		} else {
			invalidate_point(ShmAddr, task->index);
		}

		// after operating on shared memory, show all points in shared memory
		show_points(ShmAddr, MAX_NUM_POINTS);
		shm_unlock(SHM_KEY);
	} else {
		log_event(WARNING, " [%s] Skipping task due to segment lock error.", name);
	}

	/* Note: the task item is not removed from the Tasks list since it is
	possible for SIGINT to cause the list to be needed again */
}

void * thread_entry_point(void * x){
	char * name = get_thread_name();

	// announce when you have started and stopped work
	log_event(INFO, " [%s] Thread starting to process each entry", name);
	iterate_list(Tasks, process_entry);
	log_event(INFO, " [%s] Thread completed!", name);

	/* wake up main() so that it may exit */
	pthread_mutex_lock(&SyncMutex);
	pthread_cond_signal(&TaskingCompleted);
	pthread_mutex_unlock(&SyncMutex);

	// the thread library will call th_exit() for us.
	return NULL;
}


// intended to be private
static void graceful_exit() {

	/* Given that a thread is doing all of the work and main() is simply waiting
	for the thread to complete and will detach/destroy the memory segment by
	default, then the only required action is to stop the worker thread. */
	log_event(WARNING, " [MAIN] Got SIGINT or SIGQUIT! Detach, Destroy and exit...");

	th_kill_all();

	/* wake up main() so that it may exit */
	pthread_mutex_lock(&SyncMutex);
	pthread_cond_signal(&TaskingCompleted);
	pthread_mutex_unlock(&SyncMutex);
}

// intended to be private
static void clear_and_restart() {
	log_event(WARNING, " [MAIN] Got SIGHUP! Clear segment and re-install...");

	/* ensure main is retriggered to install tasks  */
	ReinstallTasks = true;

	/* REQ_install_data_6: clear shared memory segment of all data
	(not just invalidate) */
	memset(ShmAddr, 0, sizeof(Point)*MAX_NUM_POINTS);

	/* wake up main() so that it may reinstall tasks */
	pthread_mutex_lock(&SyncMutex);
	pthread_cond_signal(&TaskingCompleted);
	pthread_mutex_unlock(&SyncMutex);
}

int main(int argc, char *argv[]) {
	FILE * fp;
	char * line = NULL;
	size_t len = 0;
	sigset_t mask;

	/* just one little easter-egg that helps in testing */
	if (argc == 3 && !strcmp(argv[2], "-q")) {
		printf("Ssssshhhh, don't be so loud!\n");
		/* forget about this argument entirely */
		argc = 2;
	} else {
		also_print_log(true);
	}
	set_logfile("/var/log/install_data.log");

	/* this is not necessary, but I wanted to be explicit with a log entry */
	use_semaphores(false);

	th_use_sigint_handler(false);
	th_use_sigquit_handler(false);

	/* initially block all signals (we will unblock a few soon) */
	sigfillset(&mask);
	sigprocmask(SIG_SETMASK, &mask, NULL);

	/* REQ_install_data_7: Also, upon the receipt of a SIGTERM or SIGINT signal, the
	install_data program shall detach and destroy the shared memory segment and then
	exit. */
	th_install_signal_handler(SIGINT, graceful_exit);
	th_install_signal_handler(SIGQUIT, graceful_exit);

	/* REQ_install_data_6: Additionally, upon receipt of a SIGHUP signal, the
	install_data program should clear the shared memory area, and re-install the
	requested data as described above from the beginning of the input file. */
	th_install_signal_handler(SIGHUP, clear_and_restart);

	if (argc != 2) {
		log_event(FATAL, " [MAIN] Invalid number of arguments given");
		printf("Please provide a file path as an argument.\n");
		exit(1);
	}

	log_event(INFO, " [MAIN] Started install_data");

	/* REQ_install_data_1: Verify that the argument file is provided on the command
	line, and that the file can be opened for reading. */
	if ((fp = fopen( argv[1], "r" )) == NULL) {
		log_event (FATAL, " [MAIN] Error: Could not open file (%d): %s", errno, strerror(errno));
		exit(1);
	}

	Tasks = new_list();

	/* REQ_install_data_3: Process the data from the file, a line at a time.... */
	while (getline(&line, &len, fp) != -1) {
		create_entry(Tasks, line);
	}

	// clean up resources that will no longer be needed
	free(line);
	fclose(fp);

	log_event (INFO, " [MAIN] Completed processing input file");
	iterate_list(Tasks, show_task);

	/* REQ_install_data_2: Call connect_shm( ) which should return a pointer to the
	shared memory area. */
	ShmAddr = (void*) connect_shm(SHM_KEY, MAX_NUM_POINTS*sizeof(Point));
	shm_lock(SHM_KEY);
		show_segments();
	shm_unlock(SHM_KEY);

	if (ShmAddr == NULL) {
		log_event(FATAL, " [MAIN] Error: failed to create memory segment");
		exit(1);
	}

	/* this condition is used to determine when the tasking has been fully completed
	with no requests for restart. Since restarting means kill the thread and
	restart it then a simple pthread wait is not good enough. Instead positive
	confirmation from the worker thread is sufficient. */
	pthread_cond_init(&TaskingCompleted, NULL);
	pthread_mutex_init(&SyncMutex, NULL);

	do {
		ReinstallTasks = false;

		/* since there is mandatory signal handling and sleep() cannot be restarted
		upon being interrupted by a signal, it is necessary to either use an alarm
		or a separate thread which sleeps for the given periods. Threadding is
		preferred since restarting from a SIGHUP involves canceling the thread
		and starting it again with the new data structure. */
		if (th_execute(thread_entry_point) == THD_ERROR) {
			log_event(FATAL, " [MAIN] Error: failed to create thread");
			exit(1);
		}

		/* wait for the worker thread to positively complete */
		pthread_mutex_lock(&SyncMutex);
		pthread_cond_wait(&TaskingCompleted, &SyncMutex);
		pthread_mutex_unlock(&SyncMutex);

		/* stop processing all tasks immediately (note that the worker thread checks
		for cancellation points) */
		th_kill_all();

		if (th_wait_all() != THD_OK) {
			log_event(FATAL, " [MAIN] Error: failed to wait for threads");
		}

	} while(ReinstallTasks);

	/* since the tasks have been installed, ensure we don't attempt to handle any
	more SIGHUP signals */
	th_uninstall_signal_handler(SIGHUP);

	/* by this point all tasking has been completed, we do not want to honor any more
	sighup signals nor will we need the thread condition */
	pthread_cond_destroy(&TaskingCompleted);

	/* REQ_install_data_4: Call destroy_shm( ) to delete the shared memory segment
	from the system. (Does this need to be coordinated with the use of the shared
	memory by monitor_shm? Why or why not?)

	I guess I'm going to answer this in comments? There is no coordination needed
	since monitor_shm's attachment will still exist with the data installed by
	this application... no other app will be able to attach to this segment once it
	is destroyed, but at least those that are using the segment will continue
	to be able to use their exisiting attachments to read/modify shared memory.
	*/

	log_event(INFO, " [MAIN] Destroyed %d (return:%d)", SHM_KEY, destroy_shm(SHM_KEY));

	// ensure the list elements are cleanly destroyed before exiting
	destroy_list(Tasks);

	log_event (INFO, " [MAIN] Completed!");
	return OK;
}
