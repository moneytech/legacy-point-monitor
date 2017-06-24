MAKEFLAGS= --no-print-directory

.PHONY: all install install_local depend clean uninstall

all install install_local depend clean uninstall:
	@cd src && $(MAKE) $(MAKEFLAGS) $@

purge:
	rm -f bin/*
	rm -f lib/*
