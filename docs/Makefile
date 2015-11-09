# Makefile
#
#      Makefile for docs directory of Veil
#
#      Copyright (c) 2005 - 2015 Marc Munro
#      Author:  Marc Munro
#      License: BSD
#
#
# Do not attempt to use this makefile explicitly: its targets are available
# and should be built from the main GNUmakefile in the parent directory.
# 

.PHONY: docs_clean docs docs_check

DOCS_DIR = docs
DOCS_GARBAGE = $(garbage:%=$(DOCS_DIR)/%)


docs:	docs/html

docs/html: $(SOURCES) $(HEADERS) demo/veil_demo.png
	doxygen $(DOCS_DIR)/Doxyfile || \
	    (echo "Doxygen fails: is it installed?"; exit 2)
	@rm -f NO   # Don't know why this file is generated but it can go.

docs_clean:
	@echo Cleaning docs...
	@rm -f $(DOCS_GARBAGE) NO
	@rm -rf $(DOCS_DIR)/html

docs_distclean: docs_clean
	@rm -f Doxyfile
