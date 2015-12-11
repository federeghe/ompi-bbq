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
#include <errno.h>

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
static int orte_mig_criu_dump(pid_t fpid);
static int orte_mig_criu_finalize(void);
static char *orte_mig_criu_get_name(void);
static orte_mig_migration_state_t orte_mig_criu_get_state(void);
/*
 * Global variables
 */

orte_job_t *mig_job;
orte_process_name_t mig_orted;
orte_mig_migration_state_t mig_state;

orte_mig_base_module_t orte_mig_criu_module = {
    init,
    orte_mig_base_prepare_migration,
    orte_mig_criu_dump,
    orte_mig_base_migrate,
    orte_mig_base_fwd_info,
    orte_mig_criu_finalize,
    orte_mig_criu_get_state,
    orte_mig_criu_get_name  
};

static int init(void){
    /*TODO: checks to flag us as available*/
    mig_state = MIG_AVAILABLE;
    opal_output_verbose(0, orte_mig_base_framework.framework_output,
                "%s mig:criu: Criu module initialized.",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    return ORTE_SUCCESS;
}

static int orte_mig_criu_dump(pid_t fpid){
    int dir;
    char path[20];
    char pid[10];
    
    strcpy(path, "/tmp/");
    sprintf(pid, "%d", fpid);
    strcat(path,pid);
    
    opal_output_verbose(0,orte_mig_base_framework.framework_output,
                "%s orted:mig:criu Setting parameters", 
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    
    if(0 != mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)){
        opal_output_verbose(0,orte_mig_base_framework.framework_output,
                "%s orted:mig: Error while creating dump folder", 
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return ORTE_ERROR;
    }
    
    criu_init_opts();
    criu_set_pid(fpid);
    
    if(0 > (dir = open(path, O_DIRECTORY))){
        opal_output_verbose(0,orte_mig_base_framework.framework_output,
                "%s orted:mig:criu Error while opening folder", 
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return ORTE_ERROR;
    }
        
    criu_set_images_dir_fd(dir);

    opal_output_verbose(0,orte_mig_base_framework.framework_output,
                "%s orted:mig:criu Dumping father process", 
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    
    if(0 > criu_dump()){
        opal_output_verbose(0,orte_mig_base_framework.framework_output,
                "%s orted:mig:criu Error while dumping father process", 
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return ORTE_ERROR;
    }

    mig_state = MIG_MOVING;
    
    opal_output_verbose(0,orte_mig_base_framework.framework_output,
                "%s orted:mig:criu Father process dumped", 
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    
    //TODO: zip file, call migrate()
    
    return ORTE_SUCCESS;
}


static int orte_mig_criu_finalize(void){
    return ORTE_SUCCESS;
}


static char *orte_mig_criu_get_name(void){
    return "criu";
}

static orte_mig_migration_state_t orte_mig_criu_get_state(void){
    return mig_state;
}