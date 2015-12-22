/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2007      Los Alamos National Security, LLC.  All rights
 *                         reserved. 
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orte_config.h"

#include <stdlib.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <stdio.h>
#include <errno.h>

#include "opal/runtime/opal.h"
#include "opal/util/opal_environ.h"

#include "orte/mca/mig/mig.h"
#include "orte/mca/mig/base/base.h"


int main(int argc, char *argv[])
{

    char hostname[1024];
    gethostname(hostname, 1024);


    if (OPAL_SUCCESS != opal_init_util(&argc, &argv)) {
        fprintf(stderr, "OPAL failed to initialize -- orted aborting\n");
        exit(1);
    }

    /* we are never allowed to operate as a distributed tool,
     * so insist on the ess/tool component */
    //opal_setenv("OMPI_MCA_ess", "tool", true, &environ);

    /***************************
     * We need all of OPAL and the TOOL portion of ORTE
     ***************************/
    if (ORTE_SUCCESS != orte_init(&argc, &argv, ORTE_PROC_NON_MPI)) {
        fprintf(stderr, "ORTE failed to initialize -- orted aborting\n");
        exit(1);

    }

    fprintf(stdout, "[%s] orted-restore: inizialized.\n", hostname);

    fflush(stdout); // Avoid double print during fork

    orte_mig_base_select();

    if (OPAL_SUCCESS != orte_mig_base.active_module->restore() ) {
        fprintf(stderr, "[%s] orted-restore: failed to restore.\n", hostname);
        exit(1);
    }

    fprintf(stdout, "[%s] orted-restore: success.\n", hostname);

    return 0;
}
