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

#include "opal/mca/base/base.h"
#include "opal/util/basename.h"

#include "orte/mca/ras/base/ras_private.h"
#include "ras_bbq.h"

static int ras_bbq_register(void);
static int ras_bbq_open(void);
static int ras_bbq_close(void);
static int orte_ras_bbq_component_query(mca_base_module_t **module, int *priority);

orte_ras_bbq_component_t mca_ras_bbq_component = {
    {
        /* First, the mca_base_component_t struct containing meta
           information about the component itself */

        {
            ORTE_RAS_BASE_VERSION_2_0_0,

            /* Component name and version */
            "bbq",
            ORTE_MAJOR_VERSION,
            ORTE_MINOR_VERSION,
            ORTE_RELEASE_VERSION,

            /* Component open and close functions */
            ras_bbq_open,
            ras_bbq_close,
            orte_ras_bbq_component_query,
            ras_bbq_register
        },
        {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        }
    }
};

static int _priority;

static int ras_bbq_register(void)
{
    mca_base_component_t *c = &mca_ras_bbq_component.super.base_version;

    _priority = 100;
    (void) mca_base_component_var_register(c, "priority", "Priority of the bbq ras component",
                                           MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &_priority);
    
    opal_output_verbose(0, orte_ras_base_framework.framework_output,
                    "%s ras:bbq: BBQ registered",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    return ORTE_SUCCESS;
}

static int ras_bbq_open(void)
{
	/* TODO */
    return ORTE_SUCCESS;
}

static int ras_bbq_close(void)
{
	/* TODO */
    return ORTE_SUCCESS;
}

static int orte_ras_bbq_component_query(mca_base_module_t **module, int *priority)
{
    if (NULL == getenv("BBQUE_IP") || NULL == getenv("BBQUE_PORT"))
    {
        *priority = 0;
        *module = NULL;
        OPAL_OUTPUT_VERBOSE((2, orte_ras_base_framework.framework_output,
                         "%s ras:bbque: undefined environment variables for BBQ",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return ORTE_ERROR;
    }

    OPAL_OUTPUT_VERBOSE((2, orte_ras_base_framework.framework_output,
                         "%s ras:bbque: available for selection",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    *priority = 50;
    *module = (mca_base_module_t *) &orte_ras_bbq_module;
    return ORTE_SUCCESS;
}


