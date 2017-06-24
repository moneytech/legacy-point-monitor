/*
* Description:
*   Provide the function prototypes for log mangement
*
*   - int log_event (Levels l, const char *fmt, ...)
*    l : enum instance deflared in log_mgr.h
*    fmt : character pointer to a string containing the log format spec
*    ... : the arguments to log
*
*  The log_event( ) function will take the argument information, format
*  it into a time-tagged line of text, and append it the current log
*  file. No text logStrs should appear garbled in the log_file, and
*  regardless of the number of processes that are concurrently using
*  the log library, no logStrs should be lost upon successful execution
*  of the log_event( ) function.
*
*  The date & time field shall contain, at a minimum, the month (Jan-Dec),
*  the day of the month, and the time in HH:MM:SS (hours, minutes and seconds).
*  Day of the week, the year, and the timezone is optional.
*
*  The function log_event( ) should return OK (0) upon successful return
*  and ERROR (-1) otherwise.
*
*
*   - int set_logfile (const char *logfile_name)
*    logfile_name : name of the new log file. If not an absolute pathname,
*           the logfile will be opened in the current directory of
*           the executing process. For example, set_logfile
*           ("newlog") would use a log file called newlog in
*           the current working directory of the process; however
*           set_logfile ("/tmp/logfile") would use the file
*           logfile in the /tmp directory.
*
*  Allows the user to change the file used for the logging of logStrs
*  for a particular process.  It should not be required to call this
*  function before calling log_event(); if log_event() is called before
*  set_logfile() is invoked, a default log file, defined in the include
*  file log_mgr.h shall be used.
*
*  This function will return OK (0) if the new log file is opened
*  successfully; ERROR (-1) otherwise.
*
*
*   - void close_logfile (void)
*
*  This function shall be called whenever a logfile is to be closed.
*  At a minimum, this function should close the file descriptor associated
*  with an open logfile, if necessary.
*
*
*   - void also_print_log (bool)
*
*  Enables/disabled printing log entires to stdout (in addition to writing to
*  the log).
*/

#define DEFAULT_LOG_FMT  "%s:%s:%s"
#define DEFAULT_LOG_NAME "logfile"
#define BAD_FILE         -1
#define LOG_OK           0
#define LOG_ERROR        -1

typedef enum {INFO, WARNING, FATAL} Levels;

int log_event (Levels l, const char *fmt, ...);
int set_logfile (const char *logfile_name);
void close_logfile (void);
void also_print_log(bool);
