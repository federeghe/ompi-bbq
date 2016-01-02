/*
 * Copyright (c) 2015-2016 Politecnico di Milano.  All rights reserved. 
 * 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */


#include "orte_config.h"
#include "orte/constants.h"

#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"
#include "opal/mca/event/event.h"

#include "orte/mca/mig/base/base.h"


/* 
 */

#include "orte/mca/mig/base/static-components.h"

static int orte_mig_base_close(void);
static int orte_mig_base_open(mca_base_open_flag_t flags);

/*
 * Global variables
 */
orte_mig_base_t orte_mig_base;

static int orte_mig_base_close(void)
{
    /* Close selected component */
    if (NULL != orte_mig_base.active_module) {
        orte_mig_base.active_module->finalize();
    }

    extern orte_mig_migration_info_t* mig_info;
    if (mig_info != NULL) { // Free the resources
        free(mig_info->dst_host);
        free(mig_info);
        mig_info = NULL;
    }


    return mca_base_framework_components_close(&orte_mig_base_framework, NULL);
}

/**
 *  * Function for finding and opening either all MCA components, or the one
 *   * that was specifically requested via a MCA parameter.
 *    */
static int orte_mig_base_open(mca_base_open_flag_t flags)
{
    /* set default flags */
    orte_mig_base.active_module = NULL;

    /* Open up all available components */
    return mca_base_framework_components_open(&orte_mig_base_framework, flags);
}

MCA_BASE_FRAMEWORK_DECLARE(orte, mig, "ORTE MIGration subsystem",
                           NULL, orte_mig_base_open, orte_mig_base_close,
                           mca_mig_base_static_components, 0);
