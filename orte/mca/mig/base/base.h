/*
* Copyright (c) 2016 Politecnico di Milano, Inc.  All rights reserved.
* $COPYRIGHT$
* 
* Additional copyrights may follow
* 
* $HEADER$
*
*/

#ifndef ORTE_MIG_BASE_H
#define	ORTE_MIG_BASE_H

#include "orte/mca/mig/mig.h"
#include "orte/mca/mig/mig_types.h"
#include "orte_config.h"

BEGIN_C_DECLS

/*
 * MCA Framework
 */
ORTE_DECLSPEC extern mca_base_framework_t orte_mig_base_framework;

/* Export destination node to other modules, they may need it to send dumped process data */
ORTE_DECLSPEC extern char mig_dest[256];

/* select a component */
ORTE_DECLSPEC int orte_mig_base_select(void);

/* Functions that have to be accessible from modules */
ORTE_DECLSPEC int orte_mig_base_prepare_migration(orte_job_t *jdata,
                                            char *src_name,
                                            char *dest_name);

ORTE_DECLSPEC int orte_mig_base_fwd_info(int flag);

ORTE_DECLSPEC int orte_mig_base_migrate(char *path);

/*
 * globals that might be needed
 */
typedef struct orte_mig_base_t {
    orte_mig_base_module_t *active_module;
} orte_mig_base_t;

ORTE_DECLSPEC extern orte_mig_base_t orte_mig_base;

END_C_DECLS

#endif

