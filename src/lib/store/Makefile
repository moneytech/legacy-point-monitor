
CC = cc
CFLAGS = -g -Wall -fPIC
PROJECT_ROOT=../../..
INCLUDES = -I$(PROJECT_ROOT)/include
TARGET = libstore.a
SRCS = list.c hash_table.c point.c
OBJS = $(SRCS:.c=.o)
TAGSTARGET = tags
CTAGS = ctags -x >$(TAGSTARGET)
DEPFLAGS = -M
DEPTARGET = dependlist
LOCALINSTALLPATH = $(PROJECT_ROOT)/lib
INSTALLPATH = /usr/local/lib

.PHONY: all clean install install_local depend uninstall

all: clean $(TARGET) $(TAGSTARGET) install_local

$(TARGET): $(OBJS)
	ar crs $@ $^
	ranlib $@

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TAGSTARGET): $(SRCS)
	$(CTAGS) $(SRCS)

clean:
	$(RM) *.o $(TARGET) $(TAGSTARGET) $(DEPTARGET)

install_local: $(TARGET)
	[ -d $(LOCALINSTALLPATH) ] || mkdir $(LOCALINSTALLPATH)
	install -m 755 $(TARGET) $(LOCALINSTALLPATH)

install: $(TARGET)
	install -m 755 $(TARGET) $(INSTALLPATH)

uninstall:
	rm -f $(INSTALLPATH)/$(TARGET)

depend: $(SRCS)
	$(CC) $(DEPFLAGS) $(CFLAGS) $(INCLUDES) $^ > $(DEPTARGET)

# This approach is preferred, however this is not compatible with some versions
# of make that will be run for this project. This is why gmake is insisted
# when on Solaris.
-include "$(DEPTARGET)"
