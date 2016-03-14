# Makefile
#
#      Makefile for the demo database components
#
#      Copyright (c) 2005 - 2016 Marc Munro
#      Author:  Marc Munro
#      License: BSD
#
# Do not attempt to use this makefile directly: its targets are available
# and should be built from the main GNUmakefile in the parent directory.

# ----------
# 
.PHONY: demo_all demo_clean demo dropdemo

DEMO_DIR = demo
DEMO_GARBAGE = $(garbage:%=$(DEMO_DIR)/%)

# This generates the xml definition of the demo database directly from the
# database.  Or would, if it worked properly.
#demo.xml: $(shell ls demo/*sql)
#	-$(DB2XML) | awk '/<schema/, /<\/schema>/' > $@

# Autograph from cf consulting.  Generates an input file for dot from an 
# xml schema definition
AUTOGRAPH_XSL = $(HOME)/bin/autograph.xsl
AUTOGRAPH = $(XSLTPROC) --xinclude $(AUTOGRAPH_XSL)
DOT2PNG = dot -Tpng 

demo/veil_demo.png: demo/veil_demo.xml
	$(AUTOGRAPH) demo/veil_demo.xml >demo/demo.dot
	$(DOT2PNG) demo/demo.dot >$@ || (rm $@; exit 2)
	@rm -f demo/demo.dot

# Clean this directory and ensure regression test db is removed.
demo_clean:
	rm -f $(DEMO_GARBAGE) $(DEMO_DIR)/demo_build.log

$(VEIL_DEMO_CONTROL): demo/veil_demo.sqs
	@echo Creating $(VEIL_DEMO_CONTROL)
	@cp $< $@

all: $(VEIL_DEMO_CONTROL)


demo_distclean: demo_clean
