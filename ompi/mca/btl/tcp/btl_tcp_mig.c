#include "ompi/mca/btl/tcp/btl_tcp_mig.h"
#include "ompi/mca/btl/base/btl_base_mig.h"

#include "btl_tcp.h"
#include "btl_tcp_proc.h"
#include "btl_tcp_endpoint.h"

#include "ompi/proc/proc.h"
#include "opal/mca/db/db.h"
#include "opal/mca/if/base/base.h"
#include "opal/util/net.h"
#include "orte/mca/grpcomm/base/base.h"


static void mca_btl_tcp_mig_prepare(struct mca_btl_base_module_t* btl, mca_btl_base_mig_info_t* mig_info);
static void mca_btl_tcp_mig_exec(struct mca_btl_base_module_t* btl, mca_btl_base_mig_info_t* mig_info);
static int  mca_btl_tcp_mig_restore(struct mca_btl_base_module_t* btl, mca_btl_base_mig_info_t* mig_info);
static int  mca_btl_tcp_mig_refresh_addrs(mca_btl_tcp_proc_t* proc);
static void mca_btl_tcp_mig_restore_aft_modex(opal_buffer_t *data, void *cbdata);

bool mca_btl_tcp_mig_is_ep_migrating(mca_btl_base_endpoint_t *endpoint);
void mca_btl_tcp_mig_freeze_endpoint(mca_btl_base_endpoint_t *endpoint);
void mca_btl_tcp_mig_close_endpoint (mca_btl_base_endpoint_t *endpoint);

extern mca_base_framework_t opal_if_base_framework;
int mca_btl_tcp_component_exchange(void); // In TCP component



ompi_rte_collective_t* mca_btl_tcp_mig_modex_coll = NULL;

int mca_btl_tcp_mig_event(struct mca_btl_base_module_t* btl,
                                 mca_btl_base_mig_status_t mig_status,
                                 mca_btl_base_mig_info_t* mig_info) {

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

                opal_output_verbose(20, ompi_btl_base_framework.framework_output,
                                    "%s btl:tcp: BTL_MIG_EXEC_AFTER_MIGRATION.",
                                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

                // If I am the migrating node, just update my interfaces information
                while (opal_if_base_framework.framework_refcnt > 0) {
                    // I have to close all instances across Open MPI
                    if (OPAL_UNLIKELY(OMPI_SUCCESS != mca_base_framework_close(&opal_if_base_framework))) {
                        opal_output_verbose(0, ompi_btl_base_framework.framework_output,
                                            "%s btl:tcp: Unable to close the IF framework (ignoring).",
                                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
                        break;
                    }
                }
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

        if (mca_btl_tcp_mig_is_ep_migrating(endpoint)) {
            // remove the IP addresses from the database (I will receive the
            // new ones later).
            char *key;
            key = mca_base_component_to_string(&mca_btl_tcp_component.super.btl_version);
            ompi_rte_db_remove(&endpoint->endpoint_proc->proc_ompi->proc_name,key);
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

    int rc;

    opal_output_verbose(20, ompi_btl_base_framework.framework_output,
                        "%s btl:tcp: mca_btl_tcp_mig_restore called.",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /*
     * Exchange the modex information once again.
     * BTLs will have republished their modex information.
     */
    mca_btl_tcp_mig_modex_coll = malloc(sizeof(ompi_rte_collective_t));
    OBJ_CONSTRUCT(mca_btl_tcp_mig_modex_coll, ompi_rte_collective_t);
    mca_btl_tcp_mig_modex_coll->cbfunc = mca_btl_tcp_mig_restore_aft_modex;
    mca_btl_tcp_mig_modex_coll->cbdata = btl;
    mca_btl_tcp_mig_modex_coll->id = orte_process_info.peer_mig_modex;
    mca_btl_tcp_mig_modex_coll->active = true;


    if (OMPI_SUCCESS != (rc = ompi_rte_modex(mca_btl_tcp_mig_modex_coll))) {
        opal_output_verbose(0, ompi_btl_base_framework.framework_output,
                    "btl:tcp: mca_btl_tcp_mig_restore(): Failed ompi_rte_modex() = %d",
                    rc);
        return rc;
    } else {
        opal_output_verbose(50,ompi_btl_base_framework.framework_output,
                            "btl:tcp: mca_btl_tcp_mig_restore(): ompi_rte_modex success.");
    }

    return OMPI_SUCCESS;
}

static void mca_btl_tcp_mig_restore_aft_modex(opal_buffer_t *data, void *cbdata) {

    int rc;
    mca_btl_tcp_proc_t *proc, *next;
    mca_btl_tcp_module_t *tcp_btl = (mca_btl_tcp_module_t *)cbdata;

    (void)data; // avoid warning

    opal_output_verbose(50,ompi_btl_base_framework.framework_output,
                        "btl:tcp: mca_btl_tcp_mig_restore_aft_modex()");

    OPAL_LIST_FOREACH_SAFE(proc, next, &tcp_btl->tcp_procs, mca_btl_tcp_proc_t) {
        if (OPAL_UNLIKELY(proc->proc_endpoint_count <= 0)) {
            continue;   // A proc with no endpoint (?)
        }

        // This is actualyl the only existent endpoint
        mca_btl_tcp_endpoint_t *first_endpoint = proc->proc_endpoints[0];

        if (MCA_BTL_TCP_FROZEN != first_endpoint->endpoint_state) {
            continue;
        }

        // Ok the proc is frozen so I need to check if I'm the migrating node or not
        if (!mca_btl_base_mig_am_i_migrating() || mca_btl_tcp_mig_is_ep_migrating(first_endpoint)) {
            // If I'm not the migrating node, I have to update my
            // database of interfaces of remote nodes.

            rc = mca_btl_tcp_mig_refresh_addrs(proc);
            if (OPAL_UNLIKELY(OMPI_SUCCESS != rc)) {
                opal_output_verbose(50,ompi_btl_base_framework.framework_output,
                                    "btl:tcp: mca_btl_tcp_mig_refresh_addrs() failed with code %d", rc);

                return;
            }

        }

        for (unsigned int i=0; i<proc->proc_endpoint_count; i++) {
            // For all frozen endpoint, set it to closed, so
            // if someone want to send it has to open a new connection
            // as if it was never opened.
            proc->proc_endpoints[i]->endpoint_state = MCA_BTL_TCP_CLOSED;            

            if(opal_list_get_size(&proc->proc_endpoints[i]->endpoint_frags) > 0 || proc->proc_endpoints[i]->endpoint_send_frag != NULL) {
                // But, if the endpoint has some frags to send, connect immediately.
                mca_btl_tcp_endpoint_start_connect(proc->proc_endpoints[i]);
            }
        }
    }


}


static  int mca_btl_tcp_mig_refresh_addrs(mca_btl_tcp_proc_t* proc) {

    unsigned int i;
    int rc;
    size_t size;
    struct mca_btl_tcp_addr_t* new_addresses;
    struct sockaddr_storage sockaddr;   // Debugging purpose only

#if OMPI_PROC_ENDPOINT_TAG_MAX > 1
    #warning Open MPI configured with multiple endpoints per process, migration may not work.
#endif


    // Receive the new addresses from the modex
    rc = ompi_modex_recv( &mca_btl_tcp_component.super.btl_version,
                                  proc->proc_ompi,
                                  (void**)&new_addresses,
                                  &size );

    if (OPAL_UNLIKELY(rc != OMPI_SUCCESS)) {
        opal_output_verbose(0, ompi_btl_base_framework.framework_output,
                            "%s btl:tcp: mca_btl_tcp_mig_restore: ompi_modex_recv error with status %d.",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), rc);
        return rc;
    }

    size /= sizeof(mca_btl_tcp_addr_t);

    mca_btl_tcp_proc_tosocks(proc->proc_endpoints[0]->endpoint_addr, &sockaddr);
    opal_output_verbose(50, ompi_btl_base_framework.framework_output,
                        "btl: tcp: mca_btl_tcp_mig_restore: STARTFOR proc %s, endpoint old addr %s on port %d",
                        OMPI_NAME_PRINT(&proc->proc_ompi->proc_name),
                        opal_net_get_hostname((struct sockaddr*) &(sockaddr)),
                        ntohs(proc->proc_endpoints[0]->endpoint_addr->addr_port)
    );
    opal_output_verbose(50, ompi_btl_base_framework.framework_output,
                        "btl: tcp: mca_btl_tcp_mig_restore: Previous endpoint has #nfrags=%lu,sfrag=%d,rfrag=%d",
                        opal_list_get_size(&proc->proc_endpoints[0]->endpoint_frags),
                        proc->proc_endpoints[0]->endpoint_send_frag != NULL,
                        proc->proc_endpoints[0]->endpoint_recv_frag != NULL
    );

    // Forall new addresses we have to convert the OMPi family
    // to the standard linux one
    for (i = 0; i < size; i++) {
        if (MCA_BTL_TCP_AF_INET == new_addresses[i].addr_family) {
            new_addresses[i].addr_family = AF_INET;
        }
#if OPAL_ENABLE_IPV6
        if (MCA_BTL_TCP_AF_INET6 == new_addresses[i].addr_family) {
            new_addresses[i].addr_family = AF_INET6;
        }
#endif
    }

    // Now I have to transfer the cache and waiting packets
    // to new endpoints
    for (i=0; i<proc->proc_endpoint_count; i++) {
        free(proc->proc_endpoints[i]->endpoint_proc->proc_addrs);
        proc->proc_addrs      = new_addresses;
        proc->proc_addr_count = size;
        proc->proc_endpoints[i]->endpoint_addr = new_addresses;

        // TODO: manage multiple endpoints
        break; // proc->proc_endpoint_count should be 1
    }

    mca_btl_tcp_proc_tosocks(new_addresses, &sockaddr);
    opal_output_verbose(50, ompi_btl_base_framework.framework_output,
                        "btl: tcp: mca_btl_tcp_mig_restore: AFTERFOR proc %s, endpoint new addr %s on port %d",
                        OMPI_NAME_PRINT(&proc->proc_ompi->proc_name),
                        opal_net_get_hostname((struct sockaddr*) &(sockaddr)),
                        ntohs(proc->proc_endpoints[0]->endpoint_addr->addr_port));


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

    struct sockaddr_storage sockaddr;   // Debugging purpose only
    mca_btl_tcp_proc_tosocks(endpoint->endpoint_addr, &sockaddr);

    opal_output_verbose(50, ompi_btl_base_framework.framework_output,
                        "btl: tcp: Freezing endpoint, proc %s, addr %s on port %d",
                        OMPI_NAME_PRINT(&endpoint->endpoint_proc->proc_ompi->proc_name),
                        opal_net_get_hostname((struct sockaddr*) &(sockaddr)),
                        ntohs(endpoint->endpoint_addr->addr_port));

    if (endpoint->endpoint_state == MCA_BTL_TCP_CONNECTED) {

        // Close the send side of the socket
        shutdown(endpoint->endpoint_sd, SHUT_WR);

        // The endpoint is set to blocking: the recv must wait
        // until all the data arrives.
        mca_btl_tcp_endpoint_set_blocking(endpoint, true);
    }

    endpoint->endpoint_state = MCA_BTL_TCP_FROZEN;

}

void mca_btl_tcp_mig_close_endpoint(mca_btl_base_endpoint_t *endpoint) {
    // Close the socket

    opal_event_del(&endpoint->endpoint_recv_event);
    opal_event_del(&endpoint->endpoint_send_event);
    shutdown(endpoint->endpoint_sd, SHUT_RD);
    close(endpoint->endpoint_sd);
    endpoint->endpoint_sd = -1;

}
