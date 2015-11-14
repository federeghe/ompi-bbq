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

#include "mig_criu.h"
#include "orte/mca/mig/base/base.h"
#include "orte/mca/mig/mig_types.h"

static int init(void);
static int orte_mig_criu_prepare_migration(orte_job_t *jdata,
                                char *src_name,
                                char *dest_name);
static int orte_mig_criu_migrate(orte_job_t *jdata,
                                char *src_name,
                                char *dest_name);
static int orte_mig_criu_finalize(void);

/*
 * Global variable
 */
orte_mig_base_module_t orte_mig_criu_module = {
    init,
    orte_mig_criu_prepare_migration,
    orte_mig_criu_migrate,
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

static int orte_mig_criu_migrate(orte_job_t *jdata,
                                char *src_name,
                                char *dest_name){
    return ORTE_SUCCESS;
}

static int orte_mig_criu_finalize(void){
    return ORTE_SUCCESS;
}