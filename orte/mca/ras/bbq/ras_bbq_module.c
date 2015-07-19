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

#include "netinet/in.h"
#include "orte/mca/state/state.h"

#include "orte_config.h"
#include "orte/constants.h"
#include "orte/types.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/runtime/orte_globals.h"
#include "orte/util/name_fns.h"

#include "orte/mca/ras/base/ras_private.h"
#include "ras_bbq.h"
#include "bbq_ompi_types.h"


static int init(void);
static int recv_data(int fd, short args, void *cbdata);
static int orte_ras_bbq_allocate(orte_job_t *jdata, opal_list_t *nodes);
static int finalize(void);
static void clear_nodes_list(opal_list_t *nodes);
static int send_cmd(orte_job_t *jdata);


/*
 * Global variable
 */
orte_ras_base_module_t orte_ras_bbq_module = {
    init,
    orte_ras_bbq_allocate,
    NULL,
    finalize
};

static int socket_fd;
static opal_event_t recv_ev;
static opal_list_t *received_nodes;
static orte_job_t *received_job;
static bool cmd_received;

static int init(void){
    
    short bbque_port;
    char *bbque_addr;
    struct sockaddr_in addr;
    
    if (0 == (bbque_port = atoi(getenv("BBQUE_BACON_PORT"))))
    {
        printf("bbq:module:BBQUE_BACON_PORT variable not set\n");
        return ORTE_ERROR;
    }
    
    if (NULL == (bbque_addr = getenv("BBQUE_BACON_IP")))
    {
        printf("bbq:module:BBQUE_BACON_IP variable not set\n");
        return ORTE_ERROR;
    }
    
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(bbque_addr);
    addr.sin_port = htons(bbque_port);
    
    if(connect(socket_fd,(struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        printf("bbq:module:Can't connect to bbque\n");
        return ORTE_ERROR;
    }
    
    opal_event_set(orte_event_base, &recv_ev, socket_fd, OPAL_EV_READ, recv_data, NULL);
    opal_event_add(&recv_ev, 0);
    
    OBJ_CONSTRUCT(&received_nodes, opal_list_t);
    
    printf("bbq:module:Module initialized\n");
    
    return ORTE_SUCCESS;
}

static int recv_data(int fd, short args, void *cbdata)
{
    local_bbq_cmd_t response_cmd;
    local_bbq_res_item_t response_host;
    orte_node_t *temp;
    
    
    if(!cmd_received)
    {
        if(0>read(socket_fd,&response_cmd,sizeof(local_bbq_cmd_t)))
        {
            printf("bbq:module:Error while reading command\n");
            return ORTE_ERROR;
        }
        if(response_cmd.cmd_type!= BBQ_CMD_NODES_REPLY)
        {
            printf("bbq:module:Bad command received\n");
            return ORTE_ERROR;
        }
        cmd_received=true;
    }
    else
    {
        if(0>read(socket_fd,&response_host,sizeof(local_bbq_res_item_t)))
        {
            printf("bbq:module:Error while reading host\n");
            return ORTE_ERROR;
        }
        if(response_host.more_items==0)
        {
            ORTE_ACTIVATE_JOB_STATE(received_job, ORTE_JOB_STATE_ALLOCATION_COMPLETE);
            printf("bbq:module:Job allocation complete!\n");
            cmd_received=false;
        }
        else
        {
            temp = OBJ_NEW(orte_node_t);
            strcpy(temp->name,response_host.hostname);
            temp->slots=response_host.slots_available;
            opal_list_append(received_nodes, &temp->super);
            printf("bbq:module:Node %s appended to the list\n",temp->name);
        }
        
    }
    return ORTE_SUCCESS;
    
    
    
    
    
}

static int orte_ras_bbq_allocate(orte_job_t *jdata, opal_list_t *nodes)
{
    clear_nodes_list(nodes);
    received_nodes=nodes;
    received_job=jdata;
    
    send_cmd(jdata);
    
    return ORTE_ERR_ALLOCATION_PENDING;
}

static int send_cmd(orte_job_t *jdata)
{
    local_bbq_cmd_t command;
    local_bbq_job_t job;
    
    command.cmd_type=BBQ_CMD_NODES_REQUEST;
    
    printf("bbq:module:Sending command 0...\n");
    
    if(0>write(socket_fd,&command,sizeof(local_bbq_cmd_t)))
    {
        printf("bbq:module:Error while sending command\n");
        return ORTE_ERROR;
    }
    
    job.jobid=jdata->jobid;
    job.slots_requested=jdata->num_procs;
    
    printf("bbq:module:Sending job %d with %d numprocs...\n",job.jobid,job.slots_requested);
    
    if(0>write(socket_fd,&job,sizeof(local_bbq_job_t)))
    {
        printf("bbq:module:Error while sending job\n");
        return ORTE_ERROR;
    }
    
    printf("bbq:module:Everything sent, waiting for BBQ to answer...\n");
    
    return ORTE_SUCCESS;
}

static int finalize(void)
{
    opal_event_del(&recv_ev);
    
    OBJ_DESTRUCT(received_job);
    OBJ_DESTRUCT(received_nodes);
    
    shutdown(socket_fd, 2);
    close(socket_fd);
    
    return ORTE_SUCCESS;
}

static void clear_nodes_list(opal_list_t *nodes)
{
    printf("bbq:module:Clearing node list before calling BBQUE...\n");
    while(!opal_list_is_empty(nodes))
    {
        opal_list_remove_first(nodes);
    }
    printf("bbq:module:Node list cleaned\n");
}