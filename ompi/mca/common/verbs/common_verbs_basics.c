/*
 * Copyright (c) 2012 Cisco Systems, Inc.  All rights reserved.
 *
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* This is crummy, but <infiniband/driver.h> doesn't work on all
   platforms with all compilers.  Specifically, trying to include it
   on RHEL4U3 with the PGI 32 bit compiler will cause problems because
   certain 64 bit types are not defined.  Per advice from Roland D.,
   just include the one prototype that we need in this case
   (ibv_get_sysfs_path()). */
#include <infiniband/verbs.h>
#ifdef HAVE_INFINIBAND_DRIVER_H
#include <infiniband/driver.h>
#else
const char *ibv_get_sysfs_path(void);
#endif

#include "ompi/constants.h"

#include "common_verbs.h"
#include "opal/runtime/opal_params.h"
#include "opal/util/show_help.h"
#include "ompi/mca/rte/rte.h"

/***********************************************************************/

bool ompi_common_verbs_check_basics(void)
{
#if defined(__linux__)
    int rc;
    char *file;
    struct stat s;

    /* Check to see if $sysfsdir/class/infiniband/ exists */
    asprintf(&file, "%s/class/infiniband", ibv_get_sysfs_path());
    if (NULL == file) {
        return false;
    }
    rc = stat(file, &s);
    free(file);
    if (0 != rc || !S_ISDIR(s.st_mode)) {
        return false;
    }
#endif

    /* It exists and is a directory -- good enough */
    return true;
}

int opal_common_verbs_fork_test(void)
{
    int ret = OPAL_SUCCESS;

    /* Make sure that ibv_fork_init is the first ibv_* function to be
       invoked in this process. */
#ifdef HAVE_IBV_FORK_INIT
    if (0 != ompi_common_verbs_want_fork_support) {
        /* Check if fork support is requested by the user */
        if (0 != ibv_fork_init()) {
            /* If the opal_common_verbs_want_fork_support MCA
             * parameter is >0 but the call to ibv_fork_init() failed,
             * then return an error code.
             */
            if (ompi_common_verbs_want_fork_support > 0) {
                opal_show_help("help-opal-common-verbs.txt",
                               "ibv_fork_init fail", true,
                               ompi_process_info.nodename, errno,
                               strerror(errno));
                ret = OPAL_ERROR;
            }
        }
    }
#endif

    /* Now rgister any necessary fake libibverbs drivers.  We
       piggyback loading these fake drivers on the fork test because
       they must be loaded before ibv_get_device_list() is invoked. */
    opal_common_verbs_register_fake_drivers();

    return ret;
}
