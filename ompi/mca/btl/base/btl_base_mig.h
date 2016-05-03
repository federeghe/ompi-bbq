/*
 * Copyright (c) 2016 Politecnico di Milano
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MCA_BTL_BASE_MIG_H
#define MCA_BTL_BASE_MIG_H

#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"
#include "ompi/mca/rte/rte.h"


#if ORTE_ENABLE_MIGRATION

/* Migration-related constants*/

typedef enum mca_btl_base_mig_status_e {
    BTL_MIG_RUNNING,
    BTL_MIG_PREPARE,
    BTL_MIG_EXEC,
    BTL_MIG_EXEC_AFTER_MIGRATION,   // Only for migrating procs
    BTL_MIG_DONE
} mca_btl_base_mig_status_t;

typedef struct mca_btl_base_mig_info_s {
    ompi_jobid_t jobid;
    ompi_vpid_t src_vpid;   /* The migrating daemon vpid! */
    char* src_hostname;
    char* dst_hostname;

} mca_btl_base_mig_info_t;

OMPI_DECLSPEC extern void mca_btl_base_mig_init(void);
OMPI_DECLSPEC extern bool mca_btl_base_mig_am_i_migrating(void);
OMPI_DECLSPEC extern bool mca_btl_base_mig_is_migrating(ompi_vpid_t vpid);


#endif // ORTE_ENABLE_MIGRATION

#endif // MCA_BTL_BASE_MIG_H
