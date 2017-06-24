/*
* Description:
*
* The monitor_shm program shall take one optional argument. This argument, if
* present, would be an integer which represents the amount of time in seconds to
* monitor the shared memory segment. If the argument is not present, 30 seconds
* will be the default value.
*
* Approximately each second, this program will print a line of information to the
* screen about the contents of the shared memory. Information to be reported
* includes:
*
* - Count of the active array elements (i.e. the number of elements which are valid)
* - The average x value over the active array elements, and
* - The average y value over the active array elements.
*
* Before monitor_shm exits, it shall detach (but not destroy) the shared memory
* segment.
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
#include "hash_table.h"
#include "list.h"
#include "shared_mem.h"
#include "point.h"

#define SHM_KEY           8675309
#define DEFAULT_DURATION  600
#define ERROR             -1
#define OK                0

/* The shared memory address to install the data will be stored here */
void* ShmAddr;
bool Running = true;


// intended to be private
static void signal_exit() {
	Running = false;
	log_event(WARNING, " [MAIN] Got SIGINT or SIGQUIT! Detaching and exiting...");

	/* note that sleep is interrupted on signals, so this will break the loop
	on the next iteration (immediately) */
}


void install_signal_handler(int signum, void* handler) {
	sigset_t sigset;
	struct sigaction sa;

	/* the main thread may or may not be blocking signals, we should attempt to
	unblock the ones of interest to this library */
	if (sigemptyset(&sigset) || sigaddset(&sigset, signum)){
		log_event(WARNING, " [MAIN] Error: Failed to initialize the signal mask (%d)", signum);
		return;
	}
	if (sigprocmask(SIG_UNBLOCK, &sigset, NULL)){
		log_event(WARNING, " [MAIN] Error: Failed to unblock %d signal", signum);
		return;
	}

	// block all other signals while handling a signal
	sigfillset(&sa.sa_mask);
	sa.sa_handler = handler;
	sa.sa_flags = SA_RESTART;

	if (sigaction(signum, &sa, NULL)) {
		log_event(FATAL, " [MAIN] Error: cannot install generic handler for signal (%d)", signum);
	}
}


int main(int argc, char *argv[]) {
	sigset_t mask;
	/* REQ_monitor_2: ...If the argument is not present, 30 seconds will be the default value. */
	int seconds = DEFAULT_DURATION;

	also_print_log(true);
	set_logfile("/var/log/monitor_shm.log");

	/* this is not necessary, but I wanted to be explicit with a log entry */
	use_semaphores(false);

	/* try to behave nicely to known signals, block the rest */
	sigfillset(&mask);
	sigprocmask(SIG_SETMASK, &mask, NULL);

	install_signal_handler(SIGINT, signal_exit);
	install_signal_handler(SIGQUIT, signal_exit);

	/* REQ_monitor_1: The monitor_shm program shall take one optional argument. This
	argument, if present, would be an integer which represents the amount of time in
	seconds to monitor the shared memory segment. */
	if (argc == 2) {
		seconds = atoi(argv[1]);
	}
	if (seconds < 1) {
		log_event (FATAL, " [MAIN] Invalid argument given");
		printf("Invlid argument: given seconds should be > 0\n");
		exit(ERROR);
	}

	/* connect to (and possibly create) the shared memory segment */
	ShmAddr = (void*) connect_shm(SHM_KEY, MAX_NUM_POINTS*sizeof(Point));
	shm_lock(SHM_KEY);
		show_segments();
	shm_unlock(SHM_KEY);

	if (ShmAddr == NULL) {
		log_event(FATAL, "Error: failed to create memory segment!");
		exit(1);
	}

	log_event(INFO, " [MAIN] Monitoring for the next %d seconds", seconds);
	while (seconds > 0 && Running == true) {
		log_event(INFO, " [MAIN] %d seconds left", seconds);

		/* REQ_monitor_3 is fulfulled by show_points() */
		if (shm_lock(SHM_KEY) == false) {
			log_event(WARNING, " [MAIN] The lock has been lost! Accessing the shared memory segment is potentially dangerous.");

			/* though, to ensure I am fulfilling the requirement, I will show the points anyway */
			show_points(ShmAddr, MAX_NUM_POINTS);
		} else {
			show_points(ShmAddr, MAX_NUM_POINTS);
			shm_unlock(SHM_KEY);
		}

		sleep(1);

		seconds -= 1;
	}

	/* REQ_monitor_4: Before monitor_shm exits, it shall detach (but not destroy)
	the shared memory segment. */
	log_event(INFO, " [MAIN] Detaching from %d", SHM_KEY);
	detach_shm(ShmAddr);

	log_event (INFO, " [MAIN] Completed!");
	return OK;
}
