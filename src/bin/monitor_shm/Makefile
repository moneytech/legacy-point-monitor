CC = cc
CFLAGS = -g -Wall -fPIC
PROJECT_ROOT=../../..
INCLUDES = -I$(PROJECT_ROOT)/include
TARGET = monitor_shm
SRCS = monitor_shm.c
OBJS = $(SRCS:.c=.o)
LFLAGS = -L$(PROJECT_ROOT)/lib
LIBS = -llog_mgr -lthread_mgr -lshm -lstore
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=26683
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	LIBS += -pthread
endif
ifeq ($(UNAME_S),SunOS)
	LIBS += -pthreads
endif
TAGSTARGET = tags
CTAGS = ctags -x >$(TAGSTARGET)
DEPFLAGS = -M
DEPTARGET = dependlist
LOCALINSTALLPATH = $(PROJECT_ROOT)/bin
INSTALLPATH = /usr/local/bin
LOGPATH = /var/log

.PHONY: all clean install install_local depend cleandeps uninstall

all: clean $(TARGET) $(TAGSTARGET) install_local

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TAGSTARGET): $(SRCS)
	$(CTAGS) $(SRCS)

clean:
	$(RM) *.o $(TARGET) $(TAGSTARGET) $(DEPTARGET) core *.log

install_local: $(TARGET)
	[ -d $(LOCALINSTALLPATH) ] || mkdir $(LOCALINSTALLPATH)
	install -cs -m 755 $(TARGET) $(LOCALINSTALLPATH)

install: $(TARGET)
	install -m 755 $(TARGET) $(INSTALLPATH)
	touch $(LOGPATH)/monitor_shm.log
	chown nobody $(LOGPATH)/monitor_shm.log

uninstall:
	rm -f $(INSTALLPATH)/$(TARGET) $(LOGPATH)/monitor_shm.log

depend: $(SRCS)
	$(CC) $(DEPFLAGS) $(CFLAGS) $(INCLUDES) $^ > $(DEPTARGET)

# This approach is preferred, however this is not compatible with some versions
# of make that will be run for this project. This is why gmake is insisted
# when on Solaris.
-include "$(DEPTARGET)"
