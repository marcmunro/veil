# Makefile
#
#      Makefile for regress directory of Veil
#
#      Copyright (c) 2005 - 2011 Marc Munro
#      Author:  Marc Munro
#      License: BSD
#
# Do not attempt to use this makefile explicitly: its targets are available
# and should be built from the main GNUmakefile in the parent directory.
# 

.PHONY: regress check

REGRESS_DIR = regress
REGRESS_CLEAN = $(garbage:%=$(REGRESS_DIR)/%)


# Run regression test
regress check: all
	@cd $(REGRESS_DIR) 2>/dev/null; \
	 ./regress.sh

clean: regress_clean

regress_clean:
	@cd $(REGRESS_DIR) 2>/dev/null; ./regress.sh -d
