/*
* Copyright (c) 2016 Politecnico di Milano, Inc.  All rights reserved.
* $COPYRIGHT$
* 
* Additional copyrights may follow
* 
* $HEADER$
*
*/

#ifndef ORTE_MCA_MIG_H
#define	ORTE_MCA_MIG_H

#include "orte_config.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/mig/mig_types.h"

BEGIN_C_DECLS

/*
 * MIG module
 */

/* Function pointers*/

/* Initialize the module */
typedef int (*orte_mig_base_module_init_fn_t)(void);

/* Prepares node migration */
typedef int (*orte_mig_base_module_prepare_migration_fn_t)(orte_job_t *jdata,
                                                  char *src_name,
                                                  char *dest_name);

/**
 * Dump migrating process.
 */
typedef int (*orte_mig_base_module_dump_fn_t)(pid_t fpid);

/**
 * Migrate process to destination node.
 */
typedef int (*orte_mig_base_module_migrate_fn_t)(char *host, char *path, int pid_to_restore);

/**
 * Forwards migration info to the framework communicating with resources manager
 */
typedef int (*orte_mig_base_module_fwd_info_fn_t)(int flag);

/**
 * Restore on remote node.
 */
typedef int (*orte_mig_base_module_restore_fn_t)(void);


/**
 *  Finalize the module
 */
typedef int (*orte_mig_base_module_finalize_fn_t)(void);

/*
 *  Returns the module name
 */
typedef char * (*orte_mig_base_module_get_name_fn_t)(void);

/*
 *  Returns the module state
 */
typedef orte_mig_migration_state_t(*orte_mig_base_module_get_state_fn_t)(void);

/**
 * MIG module structure
 */
struct orte_mig_base_module_2_0_0_t {
    /** Initialization function pointer */
    orte_mig_base_module_init_fn_t              init;
    /** Prepare migration function pointer */
    orte_mig_base_module_prepare_migration_fn_t prepare_migration;
    /** Dump function pointer */
    orte_mig_base_module_dump_fn_t              dump;
    /** Migrate function pointer */
    orte_mig_base_module_migrate_fn_t           migrate;
    /** Restore the node on the destination side */
    orte_mig_base_module_restore_fn_t           restore;
    /** Forward info function pointer */
    orte_mig_base_module_fwd_info_fn_t          fwd_info;
    /** Finalize function pointer */
    orte_mig_base_module_finalize_fn_t          finalize;
    /** State of the active module, may be needed*/
    orte_mig_base_module_get_state_fn_t         get_state;
    /** Name of the active module, to be passed to daemons*/
    orte_mig_base_module_get_name_fn_t          get_name;
};
/** Convenience typedef */
typedef struct orte_mig_base_module_2_0_0_t orte_mig_base_module_2_0_0_t;
/** Convenience typedef */
typedef orte_mig_base_module_2_0_0_t orte_mig_base_module_t;

/*
 * MIG component
 */

/**
 * Component initialization / selection
 */

struct orte_mig_base_component_2_0_0_t {
    /** Base MCA structure */
    mca_base_component_t base_version;
    /** Base MCA data */
    mca_base_component_data_t base_data;
};
/** Convenience typedef */
typedef struct orte_mig_base_component_2_0_0_t orte_mig_base_component_2_0_0_t;
/** Convenience typedef */
typedef orte_mig_base_component_2_0_0_t orte_mig_base_component_t;


#define ORTE_MIG_BASE_VERSION_2_0_0 \
  MCA_BASE_VERSION_2_0_0, \
  "mig", 2, 0, 0


/* Timestamps for overhead tests */
#define ORTE_MIG_OVERHEAD_TEST 1
        
END_C_DECLS

#endif	/* MIG_H */

