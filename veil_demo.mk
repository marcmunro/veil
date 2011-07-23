# GNUmakefile
#
#      PGXS-based makefile for veil_demo
#
#      Copyright (c) 2005 - 2011 Marc Munro
#      Author:  Marc Munro
#      License: BSD
#
# For a list of targets use make help.
# 

EXTENSION=veil_demo
DATA = $(wildcard veil_demo--*.sql)
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
