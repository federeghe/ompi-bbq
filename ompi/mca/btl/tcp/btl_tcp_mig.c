#include "ompi/mca/btl/tcp/btl_tcp_mig.h"
#include "ompi/mca/btl/base/btl_base_mig.h"

#include "btl_tcp.h"
#include "btl_tcp_proc.h"
#include "btl_tcp_endpoint.h"

#include "ompi/proc/proc.h"
#include "opal/mca/db/db.h"
#include "opal/frameworks.h"
#include "opal/util/net.h"


static void mca_btl_tcp_mig_prepare(struct mca_btl_base_module_t* btl, mca_btl_base_mig_info_t* mig_info);
static void mca_btl_tcp_mig_exec(struct mca_btl_base_module_t* btl, mca_btl_base_mig_info_t* mig_info);
static int mca_btl_tcp_mig_restore(struct mca_btl_base_module_t* btl, mca_btl_base_mig_info_t* mig_info);

bool mca_btl_tcp_mig_is_ep_migrating(mca_btl_base_endpoint_t *endpoint);
void mca_btl_tcp_mig_freeze_endpoint(mca_btl_base_endpoint_t *endpoint);
void mca_btl_tcp_mig_close_endpoint (mca_btl_base_endpoint_t *endpoint);

extern mca_base_framework_t opal_if_base_framework;
int mca_btl_tcp_component_exchange(void); // In TCP component

int mca_btl_tcp_mig_event(struct mca_btl_base_module_t* btl,
                                 mca_btl_base_mig_status_t mig_status,
                                 mca_btl_base_mig_info_t* mig_info) {


    (void) opal_frameworks; // Anti-warning

    switch(mig_status) {

        case BTL_MIG_PREPARE:
            mca_btl_tcp_mig_prepare(btl, mig_info);
        break;

        case BTL_MIG_EXEC:
            // Run an event loop: if a socket is pending for reading or writing
            // we must ensure to manage it (in case of migrating node the call is
            // blocking)
            opal_event_loop(opal_event_base, EVLOOP_ONCE | EVLOOP_NONBLOCK);

            mca_btl_tcp_mig_exec(btl, mig_info);
        break;

        case BTL_MIG_EXEC_AFTER_MIGRATION:
            if (OPAL_LIKELY(mca_btl_base_mig_am_i_migrating())) {
                // If I am the migrating node, just update my interfaces information
                mca_base_framework_close(&opal_if_base_framework);
                mca_base_framework_open(&opal_if_base_framework, 0);
                // And send to all node via the modex interface
                mca_btl_tcp_component_exchange();
            } else {
                // Should never happen
                return OPAL_ERR_NOT_SUPPORTED;
            }
        break;

        case BTL_MIG_DONE:
            return mca_btl_tcp_mig_restore(btl, mig_info);
        break;

        default:
            // Should never happen
            return OPAL_ERR_NOT_SUPPORTED;
        break;
    }


    return OMPI_SUCCESS;
}



static void mca_btl_tcp_mig_prepare(struct mca_btl_base_module_t* btl, mca_btl_base_mig_info_t* mig_info) {
    mca_btl_base_endpoint_t *endpoint, *next;
    mca_btl_tcp_module_t *tcp_btl = (mca_btl_tcp_module_t *)btl;

    opal_output_verbose(20, ompi_btl_base_framework.framework_output,
                        "%s btl:tcp: mca_btl_tcp_mig_prepare called.",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    // Go over all endpoint
    OPAL_LIST_FOREACH_SAFE(endpoint, next, &tcp_btl->tcp_endpoints, mca_btl_base_endpoint_t) {

        if (mca_btl_base_mig_am_i_migrating() || mca_btl_tcp_mig_is_ep_migrating(endpoint)) {
            // If my node is migrating or the current endpoint corresponds
            // to the migrating node, I stop sockets and close the send side
            // of the endpoint
            mca_btl_tcp_mig_freeze_endpoint(endpoint);
        }

    }

}

static void mca_btl_tcp_mig_exec(struct mca_btl_base_module_t* btl, mca_btl_base_mig_info_t* mig_info) {
    mca_btl_base_endpoint_t *endpoint, *next;
    mca_btl_tcp_module_t *tcp_btl = (mca_btl_tcp_module_t *)btl;


    // Go over all endpoint
    OPAL_LIST_FOREACH_SAFE(endpoint, next, &tcp_btl->tcp_endpoints, mca_btl_base_endpoint_t) {
        if (mca_btl_base_mig_am_i_migrating() || MCA_BTL_TCP_FROZEN == endpoint->endpoint_state ) {
            mca_btl_tcp_mig_close_endpoint(endpoint);
        }
    }
}

static int mca_btl_tcp_mig_restore(struct mca_btl_base_module_t* btl, mca_btl_base_mig_info_t* mig_info) {

    opal_output_verbose(20, ompi_btl_base_framework.framework_output,
                        "%s btl:tcp: mca_btl_tcp_mig_restore called.",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));


    mca_btl_base_endpoint_t *endpoint, *next;
    mca_btl_tcp_module_t *tcp_btl = (mca_btl_tcp_module_t *)btl;

    OPAL_LIST_FOREACH_SAFE(endpoint, next, &tcp_btl->tcp_endpoints, mca_btl_base_endpoint_t) {
        if(MCA_BTL_TCP_FROZEN == endpoint->endpoint_state) {

            if (!mca_btl_base_mig_am_i_migrating()) {
                // If I'm not the migrating node, I have to update my
                // database of interfaces of remote nodes.
                int rc;
                size_t size;

                struct mca_btl_tcp_addr_t* new_addresses;

                // Receive the new addresses from the modex
                rc = ompi_modex_recv( &mca_btl_tcp_component.super.btl_version,
                                              endpoint->endpoint_proc->proc_ompi,
                                              (void**)&new_addresses,
                                              &size );
                if (rc != OMPI_SUCCESS) {
                    opal_output_verbose(0, ompi_btl_base_framework.framework_output,
                                        "%s btl:tcp: mca_btl_tcp_mig_restore: ompi_modex_recv error with status %d.",
                                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), rc);
                    return rc;
                }



                free(endpoint->endpoint_proc->proc_addrs);
                endpoint->endpoint_proc->proc_addrs = new_addresses;

                opal_output_verbose(50, ompi_btl_base_framework.framework_output,
                                    "btl: tcp: mca_btl_tcp_mig_restore: proc %s, endpoint old addr %s on port %d",
                                    OMPI_NAME_PRINT(&endpoint->endpoint_proc->proc_ompi->proc_name),
                                    opal_net_get_hostname((struct sockaddr*) &endpoint->endpoint_addr),
                                    ntohs(endpoint->endpoint_addr->addr_port));

                // Free the current address (will be added by proc_insert)
                free(endpoint->endpoint_addr);

                rc = mca_btl_tcp_proc_insert(endpoint->endpoint_proc, endpoint);

                // The mca_btl_tcp_proc_insert insert a new endpoint at the end of the endpoint
                // vector, however we already have this endpoint, so just remove the duplicated
                // item at the end of the vector.
                endpoint->endpoint_proc->proc_endpoint_count--;

                opal_output_verbose(50, ompi_btl_base_framework.framework_output,
                                    "btl: tcp: mca_btl_tcp_mig_restore: proc %s, endpoint new addr %s on port %d",
                                    OMPI_NAME_PRINT(&endpoint->endpoint_proc->proc_ompi->proc_name),
                                    opal_net_get_hostname((struct sockaddr*) &endpoint->endpoint_addr),
                                    ntohs(endpoint->endpoint_addr->addr_port));


            }


            // For all frozen endpoint, set it to closed, so
            // if someone want to send it has to open a new connection
            // as if it was never opened.
            endpoint->endpoint_state = MCA_BTL_TCP_CLOSED;



            if(opal_list_get_size(&endpoint->endpoint_frags) > 0 || endpoint->endpoint_send_frag != NULL) {
                // But, if the endpoint has some frags to send, connect immediately.
                mca_btl_tcp_endpoint_start_connect(endpoint);
            }

        } // MCA_BTL_TCP_FROZEN == endpoint->endpoint_state
    } // foreach

    return OMPI_SUCCESS;
}

/**
 * @brief returns true if the node associated to endpoint is migrating, false otherwise.
 * @param endpoint the endpoint to check
 * @return
 */
bool  mca_btl_tcp_mig_is_ep_migrating(mca_btl_base_endpoint_t *endpoint) {
    int ret;
    ompi_vpid_t *vptr, vpid;

    vptr = &vpid;   // it will contain the endpoint of the parent orted

    // Get the id of the orted refered by the endpoint
    if (OMPI_SUCCESS != (ret = opal_db.fetch((opal_identifier_t*)
        &endpoint->endpoint_proc->proc_ompi->proc_name, OMPI_RTE_NODE_ID, (void**)&vptr, OPAL_UINT32))) {
        opal_output_verbose(0, ompi_btl_base_framework.framework_output,
            "%s btl:tcp: WARNING: vpid for migration not found in the local database.",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return false;
    }

    return mca_btl_base_mig_is_migrating(vpid);
}

void mca_btl_tcp_mig_freeze_endpoint(mca_btl_base_endpoint_t *endpoint) {

    endpoint->endpoint_state = MCA_BTL_TCP_FROZEN;

    // Close the send side of the socket
    shutdown(endpoint->endpoint_sd, SHUT_WR);

    // The endpoint is set to blocking: the recv must wait
    // until all the data arrives.
    mca_btl_tcp_endpoint_set_blocking(endpoint, true);

}

void mca_btl_tcp_mig_close_endpoint(mca_btl_base_endpoint_t *endpoint) {
    // Close the socket
    close(endpoint->endpoint_sd);
    endpoint->endpoint_sd = -1;

}
