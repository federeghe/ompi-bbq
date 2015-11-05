/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * Copyright (c) 2015-2016	   Politecnico di Milano.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MIG_CRIU_H
#define	MIG_CRIU_H

#include "orte_config.h"
#include "orte/mca/mig/mig.h"
#include "orte/mca/mig/base/base.h"

BEGIN_C_DECLS


struct orte_mig_criu_component_t {
	orte_mig_base_component_t super;
};
typedef struct orte_mig_criu_component_t orte_mig_criu_component_t;

ORTE_DECLSPEC extern orte_mig_criu_component_t mca_mig_criu_component;

ORTE_DECLSPEC extern orte_mig_base_module_t orte_mig_criu_module;


END_C_DECLS


#endif	/* MIG_CRIU_H */

