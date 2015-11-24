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

#include <sys/types.h>
#include <sys/socket.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "orte/mca/state/state.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/runtime/orte_globals.h"
#include "orte/util/name_fns.h"
#include "orte/mca/rmaps/base/base.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/plm/plm.h"
#include "orte/mca/plm/plm_types.h"

#include "mig_criu.h"
#include "orte/mca/mig/base/base.h"
#include "orte/mca/mig/mig_types.h"

#include "orte/mca/ras/bbq/bbq_ompi_types.h"
#include "orte/mca/ras/ras.h"
#include "orte/mca/ras/base/base.h"

static int init(void);
static int orte_mig_criu_prepare_migration(orte_job_t *jdata,
                                char *src_name,
                                char *dest_name);
static int orte_mig_criu_migrate(void);
static int orte_mig_criu_fwd_info(uint8_t flag);
static int orte_mig_criu_finalize(void);

/*
 * Global variables
 */

char mig_src[256];
char mig_dest[256];
orte_job_t *mig_job;
orte_process_name_t mig_orted;

orte_mig_base_module_t orte_mig_criu_module = {
    init,
    orte_mig_criu_prepare_migration,
    orte_mig_criu_migrate,
    orte_mig_criu_fwd_info,
    orte_mig_criu_finalize,
};

static int init(void){
    /*TODO: checks to flag us as available*/
    orte_mig_base.active_module->state = MIG_AVAILABLE;
    opal_output_verbose(0, orte_mig_base_framework.framework_output,
                "%s mig:criu: Criu module initialized.",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    return ORTE_SUCCESS;
}

static int orte_mig_criu_prepare_migration(orte_job_t *jdata,
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
		if (strcmp(node->name, src) == 0)
			break;
		i++;
	}

	if (OPAL_UNLIKELY(node == NULL)) {
		opal_output_verbose(10, orte_mig_base_framework.framework_output,
		                "%s mig:criu: Error: source node not found.",
		                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
		return ORTE_ERROR;
	}

	mig_orted = node->daemon->name;


    orte_plm.mig_event(ORTE_MIG_PREPARE, NULL);
    
}

static int orte_mig_criu_migrate(){
    return ORTE_SUCCESS;
}

static int orte_mig_criu_fwd_info(uint8_t flag){
    switch(flag){
        case ORTE_MIG_PREPARE_ACK_FLAG:
            orte_ras_base.active_module->send_mig_info(BBQ_CMD_MIGRATION_READY);
            break;
        default:
            opal_output_verbose(0, orte_mig_base_framework.framework_output,
                "%s mig:criu: Unknown message to forward.",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    }
    return ORTE_SUCCESS;
}

static int orte_mig_criu_finalize(void){
    return ORTE_SUCCESS;
}
