/*
 * Copyright (c) 2016 Politecnico di Milano
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "opal/include/opal_config.h"
#include "ompi/mca/btl/base/btl_base_mig.h"
#include "ompi/mca/btl/base/btl_base_error.h"
#include "ompi/mca/btl/base/base.h"
#include "orte/mca/oob/base/base.h"
#include "orte/mca/mig/mig_types.h"

#include <signal.h>
#include <stdio.h>

#if ORTE_ENABLE_MIGRATION

#define MIGRATION_SIGNAL     SIGUSR1
#define MIGRATION_ACK_SIGNAL SIGUSR1

/* ** GLOBAL VARIABLES ** */
opal_event_t*            mca_btl_base_mig_signal_event;
mca_btl_base_mig_info_t* mca_btl_base_mig_info = NULL;

/* ** PROTOTYPES ** */
static void mca_btl_base_mig_signal(int);
static bool mca_btl_base_mig_read_info(void);

/* ** FUNCTIONS ** */
void mca_btl_base_mig_init(void)
{
    opal_output_verbose(50, ompi_btl_base_framework.framework_output,
                        "%s btl:base: initializing migration event.",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    mca_btl_base_mig_signal_event = (opal_event_t*)malloc(sizeof(opal_event_t));
    opal_event_signal_set(orte_event_base, mca_btl_base_mig_signal_event,
                          MIGRATION_SIGNAL,mca_btl_base_mig_signal, NULL);
    opal_event_signal_add(mca_btl_base_mig_signal_event, 0);

}

static void mca_btl_base_mig_signal(int sig) {
    static mca_btl_base_mig_status_t current_mig_state = BTL_MIG_RUNNING; // At start, no migration in effect

    if (mca_btl_base_mig_info == NULL) {
        mca_btl_base_mig_info = malloc(sizeof(mca_btl_base_mig_info_t));
    }

    opal_output_verbose(50, ompi_btl_base_framework.framework_output,
                        "%s btl:base: in mig state %i received migration signal from my daemon.",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), current_mig_state);

    // This handler should be called only for the signal that is registered for.
    assert(sig == MIGRATION_SIGNAL);


    switch(current_mig_state) {
      mca_btl_base_selected_module_t *selected_module, *it_selected_module;
      case BTL_MIG_RUNNING:

        /* First signal, switch to MIG_PREPARE */
        current_mig_state = BTL_MIG_PREPARE;

        /* My daemon creates a file containing the information for migration */
        if (false == mca_btl_base_mig_read_info()) {
            BTL_ERROR(("Unable to open/read the migration info file."));
            return;
        }

        if (mca_btl_base_mig_am_i_migrating()) {
            // Call the under layer oob to prepare for migration. This is necessary to
            // change the address of the connection process-orted
            orte_oob_base_mig_event(ORTE_MIG_PREPARE_APP, mca_btl_base_mig_info->src_hostname);
        }

        // Foreach active BTL module, call the mig_event.
        OPAL_LIST_FOREACH_SAFE(selected_module, it_selected_module, &mca_btl_base_modules_initialized,
                               mca_btl_base_selected_module_t) {
                selected_module->btl_module->btl_mig_event(selected_module->btl_module, current_mig_state,mca_btl_base_mig_info);
        }

        // Now I can send back the ACK to my orted
        kill(getppid(), MIGRATION_ACK_SIGNAL);

      break;
      case BTL_MIG_PREPARE:
            /* Second signal, switch to MIG_EXEC */
            current_mig_state = BTL_MIG_EXEC;


            if (mca_btl_base_mig_am_i_migrating()) {
                // As before call the under layer oob to issue "EXEC"
                orte_oob_base_mig_event(ORTE_MIG_EXEC_APP, mca_btl_base_mig_info->dst_hostname);
            }

            // Foreach active BTL module, call the mig_event.
            OPAL_LIST_FOREACH_SAFE(selected_module, it_selected_module, &mca_btl_base_modules_initialized,
                                   mca_btl_base_selected_module_t) {
                    selected_module->btl_module->btl_mig_event(selected_module->btl_module, current_mig_state,mca_btl_base_mig_info);
            }

            // Now I can send back the ACK to my orted
            kill(getppid(), MIGRATION_ACK_SIGNAL);

      break;
      case BTL_MIG_EXEC:
            /* Second signal, the migration is end! */
            current_mig_state = BTL_MIG_DONE;

            if (mca_btl_base_mig_am_i_migrating()) {
                // Redo the inizilization of opal_output in order to change
                // the hostname printed for debugging purpose
                opal_output_renew_hostname();

                // As before call the under layer oob to issue "DONE"
                orte_oob_base_mig_event(ORTE_MIG_DONE_APP, NULL);

                // Send to all nodes the information about my new IPs
                OPAL_LIST_FOREACH_SAFE(selected_module, it_selected_module, &mca_btl_base_modules_initialized,
                                       mca_btl_base_selected_module_t) {
                        selected_module->btl_module->btl_mig_event(selected_module->btl_module, BTL_MIG_EXEC_AFTER_MIGRATION,mca_btl_base_mig_info);
                }

                // Wait for next signal to procede
                break;
            }

      // No break here! If I am not the migrating node, I can restart immediately BTL connections
      case BTL_MIG_DONE:

          // Now I can safely restart the btl
          // Foreach active BTL module, call the mig_event.
          OPAL_LIST_FOREACH_SAFE(selected_module, it_selected_module, &mca_btl_base_modules_initialized,
                                 mca_btl_base_selected_module_t) {
                  selected_module->btl_module->btl_mig_event(selected_module->btl_module, current_mig_state,mca_btl_base_mig_info);
          }

          current_mig_state = BTL_MIG_RUNNING;

        free(mca_btl_base_mig_info->src_hostname);
        free(mca_btl_base_mig_info->dst_hostname);
      break;
    }


}

static bool mca_btl_base_mig_read_info(void) {
    FILE *mig_info_f;
    char filename[40];

    sprintf(filename,"/tmp/orted_mig_nodes_%i",getppid());
    mig_info_f = fopen(filename,"r");

    if(NULL == mig_info_f) {
        return false;
    }

    const int max_hostname = 256 /* max DNS length allowed */;

    mca_btl_base_mig_info->src_hostname = malloc(sizeof(char)*max_hostname + 1);
    mca_btl_base_mig_info->dst_hostname = malloc(sizeof(char)*max_hostname + 1);

    fscanf(mig_info_f,"%u %u %s %s",&mca_btl_base_mig_info->jobid,
                                    &mca_btl_base_mig_info->src_vpid,
                                    mca_btl_base_mig_info->src_hostname,
                                    mca_btl_base_mig_info->dst_hostname);

    if(ferror(mig_info_f)) {
        return false;
    }

    fclose(mig_info_f);

    return true;
}

bool mca_btl_base_mig_am_i_migrating(void) {
    if (mca_btl_base_mig_info != NULL) {
        // Migration in progress...
        if (mca_btl_base_mig_info->src_vpid == OMPI_RTE_MY_NODEID) {
            // Ok my daemon is migrating, so I am migrating
            return true;
        }
    }
    return false;
}

bool mca_btl_base_mig_is_migrating(ompi_vpid_t vpid) {
    if (mca_btl_base_mig_info != NULL) {
        // Migration in progress...
        if (mca_btl_base_mig_info->src_vpid == vpid) {
            return true;
        }
    }
    return false;
}

#endif // ORTE_ENABLE_MIGRATION
