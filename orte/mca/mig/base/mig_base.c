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
 * Copyright (c) 2015	   Politecnico di Milano.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orte_config.h"
#include "orte/constants.h"
#include "orte/types.h"

#include "orte/mca/state/state.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/runtime/orte_globals.h"
#include "orte/util/name_fns.h"
#include "orte/mca/rmaps/base/base.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/plm/plm.h"
#include "orte/mca/plm/plm_types.h"
#include "orte/mca/plm/base/plm_private.h"

#include "orte/mca/mig/base/base.h"
#include "orte/mca/mig/mig_types.h"

#include "orte/mca/ras/bbq/bbq_ompi_types.h"
#include "orte/mca/ras/ras.h"
#include "orte/mca/ras/base/base.h"
#include "orte/mca/ras/ras.h"

#include "orte/mca/oob/tcp/oob_tcp.h"

char mig_src[256];
char mig_dest[256];
orte_job_t *mig_job;
orte_process_name_t mig_orted;


int orte_mig_base_prepare_migration(orte_job_t *jdata,
                                char *src_name,
                                char *dest_name){
    /* Save migration data locally */

    strcpy(mig_src, src_name);
    strcpy(mig_dest, dest_name);
    mig_job = jdata;

    // Search the source node in the pool, so we can get the info of orted.
    int i=0;
    orte_node_t* node;
    while ((node = opal_pointer_array_get_item(orte_node_pool, i)) != NULL) {
            if (strcmp(node->name, mig_src) == 0)
                    break;
            i++;
    }
    
    if (OPAL_UNLIKELY(node == NULL)) {
            opal_output_verbose(10, orte_mig_base_framework.framework_output,
                            "%s mig:base: Error: source node not found.",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            return ORTE_ERROR;
    }

    opal_output_verbose(0, orte_mig_base_framework.framework_output,
                    "%s mig:base: Preparing for migration, sending mig_event...",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    
    mig_orted = node->daemon->name;
    orte_plm.mig_event(ORTE_MIG_PREPARE, &mig_orted);
    orte_oob_base_mig_event(ORTE_MIG_PREPARE, &mig_orted);
    
    return ORTE_SUCCESS;

}

int orte_mig_base_fwd_info(int flag){
    switch(flag){
        case ORTE_MIG_PREPARE_ACK_FLAG:
            orte_ras_base.active_module->send_mig_info(BBQ_CMD_MIGRATION_READY);
            orte_plm.mig_event(ORTE_MIG_EXEC, &mig_orted);

            break;
        case ORTE_MIG_READY_FLAG:
            orte_ras_base.active_module->send_mig_info(BBQ_CMD_MIGRATION_ONGOING);
            orte_oob_base_mig_event(ORTE_MIG_EXEC, &mig_orted);
        break;
        default:
            opal_output_verbose(0, orte_mig_base_framework.framework_output,
                "%s mig:criu: Unknown message to forward.",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    }
    return ORTE_SUCCESS;
}
