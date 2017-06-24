/*
* Library: log_mgr - manage and interface with a set of logs
*/


#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/file.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include "log_mgr.h"

#define RESET  "\x1B[0m"
#define RED  "\x1B[31m"
#define GREEN  "\x1B[32m"
#define YELLOW  "\x1B[33m"
#define BLUE  "\x1B[34m"

static const char *LEVEL_STRING[] = {"INFO", "WARNING", "FATAL"};
static int Fd = BAD_FILE;
static bool AlsoPrint = false;

void also_print_log(bool print) {
	AlsoPrint = print;
}

int _log_event(const char *fmt, va_list ap) {
	int bufferSize = 2048;
	int resultSize;
	char buffer[bufferSize];
	char *logStr = buffer;
	int ret = LOG_OK;
	va_list ap_copy;

	/* attempt to format the log string from the argument list, resizing
	the buffer on the heap if necessary (and trying again). Copy the ap list
	in case this doesn't work so that we can try again. */
	va_copy(ap_copy, ap);
	resultSize = vsnprintf(logStr, bufferSize, fmt, ap);
	if (resultSize >= bufferSize) {
		bufferSize = resultSize + 1;
		logStr = (char *) malloc(bufferSize);

		resultSize = vsnprintf(logStr, bufferSize, fmt, ap_copy);
		/* check if enough size was allocated, if not then bail */
		if (resultSize >= bufferSize) {
			fprintf(stderr,
							"LOG_ERROR: Buffer too small: %s\n",
							strerror(errno));
			ret = LOG_ERROR;
		}
	}

	/* only attempt to write the formatted log string to the log if there aren't
	any errors from formatting the string */
	if (ret != LOG_ERROR) {
		int bytesWritten = write(Fd, logStr, strlen(logStr));

		/* optionally print the message to stdout; also has last minute formatting for different kinds of log lines */
		if (AlsoPrint) {
			if (strstr(logStr, "FATAL") != NULL || strstr(logStr, "Error") != NULL || strstr(logStr, "ERROR") != NULL) {
				printf("%s%s%s", RED, logStr, RESET);
			} else if (strstr(logStr, "WARNING") != NULL) {
				printf("%s%s%s", YELLOW, logStr, RESET);
			} else {
				printf("%s", logStr);
			}
		}

		if (bytesWritten == -1) {
			fprintf(stderr,
							"LOG_ERROR: Could not write to file: %s\n",
							strerror(errno));
			ret = LOG_ERROR;
		} else if (bytesWritten < strlen(logStr)) {
			fprintf(stderr,
							"LOG_ERROR: Short write: %s\n",
							strerror(errno));
			ret = LOG_ERROR;
		}
	}

	/* if the heap buffer was used for the logStr then free it */
	if (logStr != buffer) {
		free(logStr);
	}

	/* no matter what the result, we will always be done with the ap list copy
	by this point */
	va_end(ap_copy);

	return ret;
}

int log_event (Levels l, const char *fmt, ...) {
		int ret;
		int len;
		va_list ap;
		char *new_fmt;
		const char fmt_template[] = "%02d:%02d:%02d.%03d  %-7s |";

		if (Fd == BAD_FILE) {
			int open_ret = set_logfile(DEFAULT_LOG_NAME);
			if (open_ret) {
				printf("LOG_ERROR: Could not open file (log_event)\n");
			}
		}

		/* get the local time for the log line */
		time_t secs = time(0);
		int ms;
		struct timespec ms_local;
		struct tm *local = localtime(&secs);
		clock_gettime(CLOCK_REALTIME, &ms_local);

		ms = (int) (ms_local.tv_nsec / 1.0e6);


		/* find the length for the augmented format string and allocate. */
		len = snprintf(NULL, 0, fmt_template, local->tm_hour,
																					local->tm_min,
																					local->tm_sec,
																					ms,
																					LEVEL_STRING[l]);

		// +2 for newline and null termination
		len += strlen(fmt) + 2;

		new_fmt = malloc(len);

		/* add the new format string to the buffer */
		len = snprintf(new_fmt, len, fmt_template, local->tm_hour,
																			 				 local->tm_min,
																							 local->tm_sec,
																							 ms,
																							 LEVEL_STRING[l]);

		/* keep the user format and add a newline */
		strcat(new_fmt, fmt);
		strcat(new_fmt, "\n");

		va_start(ap, fmt);
		ret = _log_event(new_fmt, ap);
		va_end(ap);

		free(new_fmt);
		return ret;
}

int set_logfile (const char *logfile_name) {
	/* attempt to open the log file */
	int tmp_fd = open(logfile_name, O_CREAT | O_WRONLY | O_APPEND, 0666);
	if (tmp_fd == BAD_FILE) {
		printf("LOG_ERROR: Could not open file '%s' for writing: %s\n",
						logfile_name, strerror(errno));

		return LOG_ERROR;
	}

	/* check to see if the logfile is already open. If so, close this one
	and open the new log file. */
	close_logfile();

	/* use the new file descriptor */
	Fd = tmp_fd;

	return LOG_OK;
}

void close_logfile (void) {
	if (Fd >= 0){
		close(Fd);
		/* allow for this file to be closed, but if logging is invoked again,
	  then treat allow for the default logfile to be used */
		Fd = BAD_FILE;
	}
}
