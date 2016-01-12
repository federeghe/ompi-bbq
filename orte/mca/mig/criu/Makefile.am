# Copyright (c) 2008-2010 Cisco Systems, Inc.  All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

if ORTE_ENABLE_MIGRATION

AM_CPPFLAGS = $(mig_criu_CPPFLAGS)

dist_ompidata_DATA = help-mig-criu.txt


sources = \
        mig_criu.h \
        mig_criu_component.c \
        mig_criu_module.c


# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orte_mig_criu_DSO
lib = 
lib_sources = 
component = mca_mig_criu.la
component_sources = $(sources)
else
lib = libmca_mig_criu.la
lib_sources = $(sources)
component =
component_sources =
endif
mig_criu_LIBS = -lcriu

mcacomponentdir = $(ompilibdir)
mcacomponent_LTLIBRARIES = $(component)
mca_mig_criu_la_SOURCES = $(component_sources)
mca_mig_criu_la_LDFLAGS = -module -avoid-version $(mig_criu_LDFLAGS)
mca_mig_criu_la_LIBADD = $(mig_criu_LIBS)
mca_mig_criu_la_CPPFLAGS = $(mig_criu_CPPFLAGS)

noinst_LTLIBRARIES = $(lib)
libmca_mig_criu_la_SOURCES = $(lib_sources)
libmca_mig_criu_la_LDFLAGS = -module -avoid-version $(mig_criu_LDFLAGS)
libmca_mig_criu_la_LIBADD = $(mig_criu_LIBS)
libmca_mig_criu_la_CPPFLAGS = $(mig_criu_CPPFLAGS)

endif
