/*
* Copyright (c) 2016 Politecnico di Milano, Inc.  All rights reserved.
* $COPYRIGHT$
* 
* Additional copyrights may follow
* 
* $HEADER$
*
*/

/** MIG includes **/
#include "base.h"
#include "orte/mca/mig/mig.h"
#include "orte/mca/mig/mig_types.h"

/** ORTE includes **/
#include "orte_config.h"

/*
 * Select one MIG component from all those that are available.
 */
int orte_mig_base_select(void)
{
    /* For all other systems, provide the following support */

    orte_mig_base_component_t *best_component = NULL;
    orte_mig_base_module_t *best_module = NULL;

    /*
     * Select the best component
     */
    if( OPAL_SUCCESS != mca_base_select("mig", orte_mig_base_framework.framework_output,
                                        &orte_mig_base_framework.framework_components,
                                        (mca_base_module_t **) &best_module,
                                        (mca_base_component_t **) &best_component) ) {
        /* This will only happen if no component was selected */
        /* If we didn't find one to select, that is okay */
        return ORTE_SUCCESS;
    }

    /* Save the winner */
    orte_mig_base.active_module = best_module;
    
    return ORTE_SUCCESS;
}


//TODO: write finalize()