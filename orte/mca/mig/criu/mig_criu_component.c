/*
 * Copyright (c) 2015-2016	   Politecnico di Milano.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orte_config.h"
#include "orte/constants.h"

#include "opal/mca/base/base.h"
#include "opal/util/basename.h"

#include "mig_criu.h"
#include "orte/mca/mig/mig.h"

static int mig_criu_register(void);
static int mig_criu_open(void);
static int mig_criu_close(void);
static int orte_mig_criu_component_query(mca_base_module_t **module, int *priority);

orte_mig_criu_component_t mca_mig_criu_component = {
    {
        /* First, the mca_base_component_t struct containing meta
           information about the component itself */

        {
            ORTE_MIG_BASE_VERSION_2_0_0,

            /* Component name and version */
            "criu",
            ORTE_MAJOR_VERSION,
            ORTE_MINOR_VERSION,
            ORTE_RELEASE_VERSION,

            /* Component open and close functions */
            mig_criu_open,
            mig_criu_close,
            orte_mig_criu_component_query,
            mig_criu_register
        },
        {
            /* The component is checkpoint ready 
             * TODO: what is this???
             */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        }
    }
};

static int _priority;

static int mig_criu_register(void)
{
    mca_base_component_t *c = &mca_mig_criu_component.super.base_version;

    _priority = 100;
    (void) mca_base_component_var_register(c, "priority", "Priority of the MIG Criu component",
                                           MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &_priority);
    
    opal_output_verbose(0, orte_mig_base_framework.framework_output,
                    "%s mig:criu: Criu registered",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    return ORTE_SUCCESS;
}

static int mig_criu_open(void)
{
	/* TODO */
    return ORTE_SUCCESS;
}

static int mig_criu_close(void)
{
	/* TODO */
    return ORTE_SUCCESS;
}

static int orte_mig_criu_component_query(mca_base_module_t **module, int *priority)
{
    /**
     * There are no other components for this framework right now, so Criu must be
     * selected.  
     */
    
    OPAL_OUTPUT_VERBOSE((2, orte_mig_base_framework.framework_output,
                         "%s mig:criu: available for selection",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    *priority = 50;
    *module = (mca_base_module_t *) &orte_mig_criu_module;
    return ORTE_SUCCESS;
}



