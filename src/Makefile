
MAKEFLAGS= --no-print-directory
START_UNDERLINE=\033[4m
START_EMPHASIS=\033[1m\033[34m
START_NORMAL=\033[0m

.PHONY: all install install_local depend clean uninstall

# Note: the libs must be built before the bin, ls -d */ in reverse order. This
# convention is used on all targets for consistency.

all install install_local depend clean uninstall:
	@BASEDIR=`pwd`; \
	for theDir in $$(ls -d */ | sort -r); do \
		echo "$(START_EMPHASIS)<< Procsssessing $(START_UNDERLINE)$$BASEDIR/$$theDir$(START_NORMAL)$(START_EMPHASIS) >>$(START_NORMAL)"; \
		cd $$BASEDIR/$$theDir; $(MAKE) $(MAKEFLAGS) $@; \
	done;
