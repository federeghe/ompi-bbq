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
#include <sys/stat.h>
#include <fcntl.h>
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

#include "criu/criu.h"


static int init(void);
static int orte_mig_criu_migrate(pid_t fpid);
static int orte_mig_criu_finalize(void);

/*
 * Global variables
 */

orte_job_t *mig_job;
orte_process_name_t mig_orted;
char name[]="criu";

orte_mig_base_module_t orte_mig_criu_module = {
    init,
    orte_mig_base_prepare_migration,
    orte_mig_criu_migrate,
    orte_mig_base_fwd_info,
    orte_mig_criu_finalize,
    MIG_NULL,
    name
};

static int init(void){
    /*TODO: checks to flag us as available*/
    orte_mig_base.active_module->state = MIG_AVAILABLE;
    opal_output_verbose(0, orte_mig_base_framework.framework_output,
                "%s mig:criu: Criu module initialized.",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    char hostname[100];
    gethostname(hostname, 100);
    fprintf(stderr, "++++++++++++ I'm MIG on %s\n", hostname);
    return ORTE_SUCCESS;
}

static int orte_mig_criu_migrate(pid_t fpid){
    int dir;
    
    char hostname[100];
    gethostname(hostname, 100);
    fprintf(stderr, "++++++++++++ I'm CRIU on %s\n", hostname);
    criu_init_opts();
    criu_set_pid(fpid);

    dir = open("/tmp/dump", O_DIRECTORY);
    criu_set_images_dir_fd(dir);

    if(0 > criu_dump()){
        opal_output(0, "%s orted: Error dumping father process", 
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return;
    }

    opal_output(0, "%s orted: Daemon successfully dumped", 
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    return ORTE_SUCCESS;
}


static int orte_mig_criu_finalize(void){
    return ORTE_SUCCESS;
}
