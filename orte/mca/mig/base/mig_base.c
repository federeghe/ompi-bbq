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
#include "orte/types.h"


#include <libtar.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <fcntl.h>

#include "orte/runtime/orte_globals.h"
#include "orte/mca/plm/plm.h"

#include "orte/mca/mig/base/base.h"
#include "orte/mca/mig/mig_types.h"
#include "orte/mca/ras/base/base.h"
#include "orte/mca/ras/ras.h"

#include "orte/mca/oob/tcp/oob_tcp.h"

// The listening port for migration
#define PORT_MIGRATION_COPY 2693

// The interval between two try to connect to the destination host.
// The first attempt to connect should be successful, but it may
// happens that the ssh session on remote host should be delayed.
#define RETRY_TIMEOUT 50000

orte_mig_migration_info_t* mig_info=NULL;

static void change_hnp_internal_references(void);

int orte_mig_base_prepare_migration(orte_job_t *jdata,
                                char *src_name,
                                char *dest_name){

    if (mig_info != NULL) { // There was a previous migration, let's free the resources
        free(mig_info->src_host);
        free(mig_info->dst_host);
        free(mig_info);
        mig_info = NULL;
    }

    /* Save migration data locally */
    mig_info = malloc(sizeof(orte_mig_migration_info_t));
    mig_info->src_host = strdup(src_name);
    mig_info->dst_host = strdup(dest_name);

    // Search the source node in the pool, so we can get the info of orted.
    int i=0;
    orte_node_t* node;
    while ((node = opal_pointer_array_get_item(orte_node_pool, i)) != NULL) {
            if (strcmp(node->name, src_name) == 0)
                    break;
            i++;
    }
    
    if (OPAL_UNLIKELY(node == NULL)) {
            opal_output_verbose(10, orte_mig_base_framework.framework_output,
                            "%s mig:base: Error: source node not found.",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            return ORTE_ERROR;
    }

    opal_output_verbose(0, orte_mig_base_framework.framework_output,
                    "%s mig:base: Preparing for migration from %s to %s, sending mig_event...",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), src_name, mig_info->dst_host);

    
    mig_info->src_name = node->daemon->name;

    orte_plm.mig_event(ORTE_MIG_PREPARE, mig_info);

    orte_oob_base_mig_event(ORTE_MIG_PREPARE, &(mig_info->src_name));
    
    return ORTE_SUCCESS;

}

int orte_mig_base_fwd_info(int flag){
    switch(flag){
        case ORTE_MIG_PREPARE_ACK_FLAG:
            orte_ras_base.active_module->send_mig_info(ORTE_MIG_READY);
            orte_plm.mig_restore(mig_info->dst_host, &(mig_info->src_name));
            orte_plm.mig_event(ORTE_MIG_EXEC, mig_info);
            break;
        case ORTE_MIG_READY_FLAG:
            orte_ras_base.active_module->send_mig_info(ORTE_MIG_ONGOING);
            orte_oob_base_mig_event(ORTE_MIG_EXEC, &(mig_info->src_name));
        break;
        case ORTE_MIG_DONE_FLAG:
            orte_ras_base.active_module->send_mig_info(ORTE_MIG_DONE);
            orte_oob_base_mig_event(ORTE_MIG_DONE, (void*)(mig_info->dst_host));
            change_hnp_internal_references();
        break;
        case ORTE_MIG_ABORTED_FLAG:
            orte_ras_base.active_module->send_mig_info(ORTE_MIG_ABORTED);
            // TODO

        break;
        default:
            opal_output_verbose(0, orte_mig_base_framework.framework_output,
                "%s mig:base: Unknown message to forward.",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    }
    return ORTE_SUCCESS;
}

static void change_hnp_internal_references(void) {
    int i;
    orte_node_t *node;
    for (i=0; i < orte_node_pool->size; i++) {
        if (NULL == (node = (orte_node_t*)opal_pointer_array_get_item(orte_node_pool, i))) {
            continue;
        }
        if (0 == strcmp(mig_info->src_host, node->name)) {
            node->name = strdup(mig_info->dst_host);
            break;
        }
    }

}


/**
 * This function is called by the migrating orted child, when it's ready to pass
 * the image of checkpoint to the destination.
 */
int orte_mig_base_migrate(char *host, char *path, pid_t pid_to_restore) {
    opal_output_verbose(0, orte_mig_base_framework.framework_output,
                "%s orted:mig:base copying directory %s.",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), path);


    struct sockaddr_in addr;
    int socket_fd;
    TAR *tar;
    
    /* Open socket towards destination node to send dump directory */
    if(0 > (socket_fd = socket(AF_INET,SOCK_STREAM,0)))
    {
        opal_output_verbose(0, orte_mig_base_framework.framework_output,
                    "%s orted:mig:base Cannot create socket.",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return ORTE_ERROR;
    }
    
    // Ok, let's remove the 'user@' from the hostname
    const char* cleaned_host = strchr(host, '@');
    cleaned_host = cleaned_host == NULL ? host : cleaned_host+1;

    opal_output_verbose(0,orte_mig_base_framework.framework_output,
                "%s orted:mig:base Connecting to destination %s...",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), cleaned_host);
    
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(cleaned_host);
    addr.sin_port = htons(PORT_MIGRATION_COPY);

    while(0>connect(socket_fd,(struct sockaddr *)&addr, sizeof(addr)))
    {
        opal_output_verbose(0, orte_mig_base_framework.framework_output,
                    "%s orted:mig:base Can't connect to destination node. Retrying...",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        usleep(RETRY_TIMEOUT);
        // TODO: Implement max attempts
    }

    opal_output_verbose(0,orte_mig_base_framework.framework_output,
                "%s orted:mig:base Connected to destination node. Compressing folder...",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    // First of all send the pid of the process to restore
    write(socket_fd, (char *)&pid_to_restore,sizeof(pid_t));

    // Then send the image as tar archive.
    tar_fdopen(&tar, socket_fd, NULL, NULL, O_WRONLY, 0644, 0);
    tar_append_tree(tar, path, ".");
    close(socket_fd);

    return ORTE_SUCCESS;
}

/**
 * This method is called onto the destination node by orted-restore.
 * It receives the tar from the source node and extract all the files into
 * the directory passed to parameters.
 */
int orte_mig_base_restore(char *path) {
    opal_output_verbose(0, orte_mig_base_framework.framework_output,
                "%s orted:mig:base waiting, I will write to directory %s.",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), path);

    struct sockaddr_in addr, addr_cl;
    socklen_t size_addr_cl;
    int socket_fd, socket_cl;
    int pid_to_restore;
    TAR *tar;

    /* Open socket towards source node to recv dump directory */
    if(0 > (socket_fd = socket(AF_INET,SOCK_STREAM,0)))
    {
        opal_output_verbose(0, orte_mig_base_framework.framework_output,
                    "%s orted:mig:base Cannot create socket.",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return ORTE_ERROR;
    }

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   // Maybe in future we can restrict to source node
    addr.sin_port = htons(PORT_MIGRATION_COPY);
    
    if(0>bind(socket_fd,(struct sockaddr *)&addr, sizeof(addr)))
    {
        opal_output_verbose(0, orte_mig_base_framework.framework_output,
                    "%s orted:mig:base Can't bind local port.",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return ORTE_ERROR;
    }
    
    listen(socket_fd,1);    // Only 1 client

    size_addr_cl = sizeof(addr_cl);
    socket_cl = accept(socket_fd, (struct sockaddr *) &addr_cl, &size_addr_cl); // Wait until client arrives

    opal_output_verbose(0,orte_mig_base_framework.framework_output,
                "%s orted:mig:base Accepted source node",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    
    // First of all we receive the pid number of the parent process
    // to restore
    read(socket_cl, (char *)&pid_to_restore,sizeof(pid_t));

    opal_output_verbose(0,orte_mig_base_framework.framework_output,
                "%s orted:mig:base Decompressing folder...",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    
    // FIXME: tar does not handle socket recv correctly, it may happen
    // that the receive fails.
    tar_fdopen(&tar, socket_cl, NULL, NULL, O_RDONLY, 0644, 0);
    tar_extract_all(tar, path);
    tar_close(tar);
    close(socket_fd);
    
    return pid_to_restore;
}

