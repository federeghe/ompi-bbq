#ifndef MCA_BTL_TCP_MIG_H
#define MCA_BTL_TCP_MIG_H

#include "ompi/mca/btl/base/btl_base_mig.h"
#include "ompi/mca/btl/btl.h"

#if ORTE_ENABLE_MIGRATION

/**
 * Migration event notification function. Called by signal mgmt routine in btl_base_frame.
 * @param event Event type
 * @return OMPI_SUCCESS or failure status
 */
OMPI_DECLSPEC extern int mca_btl_tcp_mig_event(struct mca_btl_base_module_t*, mca_btl_base_mig_status_t, mca_btl_base_mig_info_t*);


#endif // ORTE_ENABLE_MIGRATION

#endif // MCA_BTL_TCP_MIG_H
