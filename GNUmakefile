# GNUmakefile
#
#      PGXS-based makefile for Veil
#
#      Copyright (c) 2005 - 2011 Marc Munro
#      Author:  Marc Munro
#      License: BSD
#
# For a list of targets use make help.
# 

# Default target.  Dependencies for this are further defined in the 
# included makefiles for SUBDIRS below.
all:

BUILD_DIR = $(shell pwd)
MODULE_big = veil
OBJS = $(SOURCES:%.c=%.o)
DEPS = $(SOURCES:%.c=%.d)
EXTRA_CLEAN = $(SRC_CLEAN)
EXTENSION=veil
MODULEDIR=extension/veil
VEIL_VERSION = $(shell \
    grep default_version veil.control | sed 's/[^0-9.]*\([0-9.]*\).*/\1/')

VEIL_CONTROL = veil--$(VEIL_VERSION).sql

SUBDIRS = src regress docs demo
include $(SUBDIRS:%=%/Makefile)


DATA = $(wildcard veil--*.sql)
ALLDOCS = $(wildcard docs/html/*)
# Only define DOCS (for the install target) if there are some.
ifneq "$(ALLDOCS)" ""
       DOCS = $(ALLDOCS)
endif

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
include $(DEPS)

# Build per-source dependency files for inclusion
# This ignores header files and any other non-local files (such as
# postgres include files).  Since I don't know if this will work
# on non-Unix platforms, we will ship veil with the dep files
# in place).  This target is mostly for maintainers who may wish
# to rebuild dep files.
%.d: %.c
	@echo Recreating $@
	@$(SHELL) -ec "$(CC) -MM -MT $*.o $(CPPFLAGS) $< | \
		xargs -n 1 | grep '^[^/]' | \
		sed -e '1,$$ s/$$/ \\\\/' -e '$$ s/ \\\\$$//' \
		    -e '2,$$ s/^/  /' | \
		sed 's!$*.o!& $@!g'" > $@

# Target used by recursive call from deps target below.  This ensures
# that make deps always rebuilds the dep files even if they are up to date.
make_deps: $(DEPS)

# Target that rebuilds all dep files unconditionally.  There should be a
# simpler way to do this using .PHONY but I can't figure out how.
deps: 
	rm -f $(DEPS)
	$(MAKE) MAKEFLAGS="$(MAKEFLAGS)" make_deps

# Define some variables for the following tarball targets.
tarball tarball_clean: VEIL_DIR=veil_$(VEIL_VERSION)

# Create a version numbered tarball of source (including deps), tools, 
# and possibly docs.
tarball:
	@rm -rf $(VEIL_DIR)
	@if test ! -r docs/html/index.html; then \
	    echo "You may want to make the docs first"; fi
	@mkdir -p $(VEIL_DIR)/src $(VEIL_DIR)/regress
	@cp CO* LI* GNU* veil--*sql veil.control veil_demo* $(VEIL_DIR)
	@cp src/*akefile src/*.[cdh] src/*sqs $(VEIL_DIR)/src
	@cp regress/*akefile regress/*.sh regress/*sql $(VEIL_DIR)/regress
	@ln -s `pwd`/tools $(VEIL_DIR)/tools
	@ln -s `pwd`/demo $(VEIL_DIR)/demo
	@ln -s `pwd`/docs $(VEIL_DIR)/docs
	@echo Creating veil_$(VEIL_VERSION).tgz...
	@tar czhf veil_$(VEIL_VERSION).tgz $(VEIL_DIR)
	@rm -rf $(VEIL_DIR)

# Cleanup after creating a tarball
tarball_clean:
	rm -rf $(VEIL_DIR) veil_$(VEIL_VERSION).tgz

# Ensure that tarball tmp files and dirs are removed by the clean target
clean: tarball_clean

# Install veil_demo as well as veil
install: demo_install
demo_install:
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) -f veil_demo.mk install

# Uninstall veil_demo as well as veil
uninstall: demo_uninstall
demo_uninstall:
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) -f veil_demo.mk uninstall

# Provide a list of the targets buildable by this makefile.
list help:
	@echo -e "\n\
 Major targets for this makefile are:\n\n\
 all       - Build veil libraries, without docs\n\
 check     - Build veil and run regression tests\n\
 clean     - remove target and object files\n\
 deps      - Recreate the xxx.d dependency files\n\
 docs      - Build veil html docs (requires doxygen and dot)\n\
 help      - show this list of major targets\n\
 install   - Install veil\n\
 list      - show this list of major targets\n\
 regress   - same as check\n\
 regress_clean - clean up after regression tests - (drop regressdb)\n\
 tarball   - create a tarball of the sources and documents\n\
 uninstall - Undo the install\n\
\n\
"
