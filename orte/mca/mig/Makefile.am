# Copyright (c) 2016 Politecnico di Milano.  All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

#if ORTE_ENABLE_MIGRATION

# main library setup
noinst_LTLIBRARIES = libmca_mig.la
libmca_mig_la_SOURCES =

# pkgdata setup
dist_ompidata_DATA =

# local files
headers = mig.h mig_types.h
libmca_mig_la_SOURCES += $(headers)

# Conditionally install the header files
if WANT_INSTALL_HEADERS
ortedir = $(ompiincludedir)/$(subdir)
nobase_orte_HEADERS = $(headers)
endif

include base/Makefile.am
#endif 

distclean-local:
	rm -f base/static-components.h
