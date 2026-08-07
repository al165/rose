# Makefile for the "rose" Pd library, using pd-lib-builder.
#
#   make                 build for the host platform
#   make install         install into $(objectsdir) (see below)
#   make clean
#
# Useful overrides:
#   make PDDIR=/path/to/pure-data          (source tree containing src/m_pd.h)
#   make PDINCLUDEDIR=/usr/include/pd      (just the headers)
#   make install objectsdir=~/.local/lib/pd/extra

# name of the library (the folder / .pd_linux basename)
lib.name = rose

# one .c file per object class; pd-lib-builder derives the class name
# from the filename and expects a matching <name>_setup() function.
class.sources = \
	display-trace.c \
	state.c

lib.setup.sources = rose.c

# extra files copied on `make install`
datafiles = \
	display-trace-help.pd \
	state-help.pd \
	README.md

# where pd-lib-builder lives
PDLIBBUILDER_DIR = pd-lib-builder/


include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder
