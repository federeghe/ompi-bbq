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
 * Copyright (c) 2015      Politecnico di Milano. All rights resevered.
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
#include "opal/util/cmd_line.h"
#include "opal/util/output.h"
#include "opal/util/argv.h"
#include "opal/util/error.h"

#include "orte/mca/mig/mig.h"
#include "orte/mca/mig/base/base.h"


opal_cmd_line_init_t cmd_line_opts[] = {

    { "orte_debug_daemons", '\0', NULL, "debug-daemons", 0,
      NULL, OPAL_CMD_LINE_TYPE_BOOL,
      "Enable debugging of OpenRTE daemons" },

    /* End of list */
    { NULL, '\0', NULL, NULL, 0,
      NULL, OPAL_CMD_LINE_TYPE_NULL,
      NULL }
};

static int parse_args(int argc, char *argv[]) {
    int i, ret, len, exit_status = ORTE_SUCCESS ;
    opal_cmd_line_t cmd_line;
    char **app_env = NULL, **global_env = NULL;
    char *argv0 = NULL;

    /* Parse the command line options */
    opal_cmd_line_create(&cmd_line, cmd_line_opts);
    mca_base_open();
    mca_base_cmd_line_setup(&cmd_line);
    ret = opal_cmd_line_parse(&cmd_line, false, argc, argv);

    if (OPAL_SUCCESS != ret) {
        if (OPAL_ERR_SILENT != ret) {
            fprintf(stderr, "%s: command line error (%s)\n", argv[0],
                    opal_strerror(ret));
        }
        exit_status = 1;
        goto cleanup;
    }

    /**
     * Put all of the MCA arguments in the environment
     */
    mca_base_cmd_line_process_args(&cmd_line, &app_env, &global_env);

    len = opal_argv_count(app_env);
    for(i = 0; i < len; ++i) {
        putenv(app_env[i]);
    }

    len = opal_argv_count(global_env);
    for(i = 0; i < len; ++i) {
        putenv(global_env[i]);
    }

    /**
     * Now start parsing our specific arguments
     */
    /* get the remaining bits */
    argv0 = strdup(argv[0]);
    opal_cmd_line_get_tail(&cmd_line, &argc, &argv);


 cleanup:
    if (NULL != argv0) {
        free(argv0);
    }

    return exit_status;
}

int main(int argc, char *argv[])
{

    char hostname[1024];
    gethostname(hostname, 1024);


    if (OPAL_SUCCESS != opal_init_util(&argc, &argv)) {
        fprintf(stderr, "OPAL failed to initialize -- orted-restore aborting\n");
        return EXIT_FAILURE;
    }

    /*
     * Parse Command Line Arguments
     */
    if (ORTE_SUCCESS != parse_args(argc, argv)) {
        fprintf(stderr, "Parsing args failed -- orted-restore aborting\n");
        return EXIT_FAILURE;
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

    int fd_output = opal_output_open(NULL);
    opal_output_set_verbosity(fd_output , 100);

    if (orte_debug_daemons_flag) {
        opal_output(0, "orted-restore: inizialized.");
        fflush(stdout); // Avoid double print during fork
    }

    orte_mig_base_select();

    if (OPAL_SUCCESS != orte_mig_base.active_module->restore() ) {
        opal_output(0, "orted-restore: failed to restore.");
        exit(1);
    }
    if (orte_debug_daemons_flag) {
        opal_output(0, "orted-restore: end of execution.");
    }
    return 0;
}
