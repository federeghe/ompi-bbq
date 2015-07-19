
# Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
#                         University Research and Technology
#                         Corporation.  All rights reserved.
# Copyright (c) 2004-2005 The University of Tennessee and The University
#                         of Tennessee Research Foundation.  All rights
#                         reserved.
# Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
#                         University of Stuttgart.  All rights reserved.
# Copyright (c) 2004-2005 The Regents of the University of California.
#                         All rights reserved.
# Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved.
# Copyright (c) 2015      Politecnico di Milano. All rights reserved.
#
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

# Use the top-level Makefile.options

dist_ortedata_DATA = help-ras-bbq.txt


AM_CPPFLAGS = $(ras_bbq_CPPFLAGS)

sources = \
        ras_bbq.h \
        ras_bbq_component.c \
        ras_bbq_module.c


# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orte_ras_bbq_DSO
lib =
lib_sources = 
component = mca_ras_bbq.la
component_sources = $(sources)
else
lib = libmca_ras_bbq.la
lib_sources = $(sources)
component =
component_sources =
endif

mcacomponentdir = $(ortelibdir)
mcacomponent_LTLIBRARIES = $(component)
mca_ras_bbq_la_SOURCES = $(component_sources)
mca_ras_bbq_la_LDFLAGS = -module -avoid-version $(ras_bbq_LDFLAGS)
mca_ras_bbq_la_LIBADD = $(ras_bbq_LIBS)

noinst_LTLIBRARIES = $(lib)
libmca_ras_bbq_la_SOURCES = $(lib_sources)
libmca_ras_bbq_la_LDFLAGS = -module -avoid-version $(ras_bbq_LDFLAGS)
libmca_ras_bbq_la_LIBADD = $(ras_bbq_LIBS)
