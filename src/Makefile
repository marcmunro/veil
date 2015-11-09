# Makefile
#
#      Makefile for src directory of Veil
#
#      Copyright (c) 2005 - 2015 Marc Munro
#      Author:  Marc Munro
#      License: BSD
#
# Do not attempt to use this makefile explicitly: its targets are available
# and should be built from the main GNUmakefile in the parent directory.
# The GNUmakefile in this directory will build using the parent GNUmakefile
# so using make <target> in this directory will work as long as you don't
# try to specify this makefile.  It even works with emacs compile and 
# next-error functions though the number of makefiles involved seems a 
# little alarming at first.
# The whole strangeness of this makefile hierarchy derives from a, 
# possibly misguided, attempt to avoid recursive make (see the article
# "Recursive make considered harmful" for a rationale).


SOURCES = src/veil_bitmap.c src/veil_config.c src/veil_datatypes.c \
	    src/veil_interface.c src/veil_mainpage.c src/veil_query.c \
	    src/veil_serialise.c src/veil_shmem.c src/veil_utils.c \
	    src/veil_variables.c

ifdef EXTENSION
	LIBDIR=$(DESTDIR)$(datadir)/extension
else
	LIBDIR=$(DESTDIR)$(pkglibdir)
endif

INSTALLED_LIB = $(LIBDIR)/$(addsuffix $(DLSUFFIX), veil)
TRIAL_LIB = $(addsuffix $(DLSUFFIX), veil_trial)
VEIL_LIB = $(addsuffix $(DLSUFFIX), veil)
HEADERS = $(wildcard src/*.h)

all: $(VEIL_CONTROL)

$(VEIL_CONTROL): src/veil_interface.sqs
	@echo Creating $(VEIL_CONTROL)
	@sed -e 's!@LIBPATH@!$$libdir/veil!g' <$< >$@

