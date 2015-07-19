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
#include "orte/mca/rmaps/base/base.h"
#include "ras_bbq.h"
#include "bbq_ompi_types.h"


static int init(void);
static int recv_data(int fd, short args, void *cbdata);
static int orte_ras_bbq_allocate(orte_job_t *jdata, opal_list_t *nodes);
static int finalize(void);
static int send_cmd(int cmd);
static int recv_nodes_reply(void);
static void launch_job(void);
static int recv_cmd(void);
static int send_cmd_node_request(void);
static int send_cmd_terminate(void);

/*
 * Global variable
 */
orte_ras_base_module_t orte_ras_bbq_module = {
    init,
    orte_ras_bbq_allocate,
    NULL,
    finalize
};

static int cmd_received;
static int socket_fd;
static opal_event_t recv_ev;
static orte_job_t *received_job;
static opal_list_t nodes;

static int init(void){
    
    short bbque_port;
    char *bbque_addr;
    struct sockaddr_in addr;
    
    
    /*Check if the environment variables needed to use BBQ are set*/
    if (0 == (bbque_port = atoi(getenv("BBQUE_BACON_PORT"))))
    {
        opal_output_verbose(0, orte_ras_base_framework.framework_output,
                    "%s ras:bbq:error: BBQUE_BACON_PORT not set.",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return ORTE_ERROR;
    }
    
    if (NULL == (bbque_addr = getenv("BBQUE_BACON_IP")))
    {
        opal_output_verbose(0, orte_ras_base_framework.framework_output,
                    "%s ras:bbq:error: BBQUE_BACON_IP not set.",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return ORTE_ERROR;
    }   
    
    /*If so, create a socket and connect to BBQ*/
    
    if(0 > (socket_fd=socket(AF_INET,SOCK_STREAM,0)))
    {
        opal_output_verbose(0, orte_ras_base_framework.framework_output,
                    "%s ras:bbq:error: Cannot create socket.",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return ORTE_ERROR;
    }
    
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(bbque_addr);
    addr.sin_port = htons(bbque_port);
    
    if(0>connect(socket_fd,(struct sockaddr *)&addr, sizeof(addr)))
    {
        opal_output_verbose(0, orte_ras_base_framework.framework_output,
                    "%s ras:bbq:error: Can't connect to BBQ.",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return ORTE_ERROR;
    }
    
    /*Tell OMPI framework to call recv_data when it receives something on socket_fd*/
    opal_event_set(orte_event_base, &recv_ev, socket_fd, OPAL_EV_READ, recv_data, NULL);
    opal_event_add(&recv_ev, 0);
    
    OBJ_CONSTRUCT(&nodes, opal_list_t);
    
    cmd_received=BBQ_CMD_NONE;
    
    opal_output_verbose(0, orte_ras_base_framework.framework_output,
                    "%s ras:bbq: Module initialized.",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    
    return ORTE_SUCCESS;
}

/*
 * This function is called by the OMPI event management system when some data is received 
 * on the socket bound during the initialization of the module. 
 * It basically loops and reads the received commands until it reaches the final message, or an error occurs.
 * TODO: timer
 */

static int recv_data(int fd, short args, void *cbdata)
{   
    while(true){
        /*cmd_received state drives the execution flow*/
        
        switch(cmd_received){
            case BBQ_CMD_NONE:
            {
                if(recv_cmd())
                {
                    return ORTE_ERROR;
                }
                break;
            }
            case BBQ_CMD_NODES_REPLY:
            {
                if(recv_nodes_reply())
                {
                    return ORTE_ERROR;
                }
                break;
            }
            case BBQ_CMD_FINISHED:
            {
                launch_job();
                
                cmd_received=BBQ_CMD_NONE;

                return ORTE_SUCCESS;
            }
            default:
            {
                opal_output_verbose(0, orte_ras_base_framework.framework_output,
                    "%s ras:bbq:error: Invalid cmd_received state.",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
                return ORTE_ERROR;
            }
        }
            
    }
}

/*This function is called internally by OMPI when mpirun command is executed*/

static int orte_ras_bbq_allocate(orte_job_t *jdata, opal_list_t *nodes)
{
    received_job=jdata;
    
    send_cmd(BBQ_CMD_NODES_REQUEST);
    
    /*
     * Since we have to wait for BBQ to send us the nodes list,
     * we notify OMPI that the allocation phase is not over yet.
     */
    return ORTE_ERR_ALLOCATION_PENDING;
}

static int send_cmd(int cmd)
{   
    switch(cmd){
        case BBQ_CMD_NODES_REQUEST:
        {
            if(send_cmd_node_request())
            {
                return ORTE_ERROR;
            }
            break;
        }
        case BBQ_CMD_TERMINATE:
        {
            if(send_cmd_terminate())
            {
                return ORTE_ERROR;
            }
            break;
        }
        default:
        {
            opal_output_verbose(0, orte_ras_base_framework.framework_output,
                "%s ras:bbq:error: Invalid command.",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            return ORTE_ERROR;
        }
    }
    return ORTE_SUCCESS;
}

static int finalize(void)
{
    send_cmd(BBQ_CMD_TERMINATE);
    
    opal_event_del(&recv_ev);
    
    OBJ_DESTRUCT(received_job);
    
    shutdown(socket_fd, 2);
    close(socket_fd);
    
    return ORTE_SUCCESS;
}


static int recv_nodes_reply(void)
{
    orte_node_t *temp;
    int bytes;
    local_bbq_res_item_t response_item;
    
    bytes=read(socket_fd,&response_item,sizeof(local_bbq_res_item_t));
    if(bytes!=sizeof(local_bbq_res_item_t))
    {
        printf("bbq:module:error: Error while reading host\n");
        return ORTE_ERROR;
    }
    
    opal_output_verbose(0, orte_ras_base_framework.framework_output,
                "%s ras:bbq: Node data received.",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    
    temp = OBJ_NEW(orte_node_t);
    
    temp->name=strdup(response_item.hostname);
    temp->slots_inuse=0;
    temp->slots_max=response_item.slots_available;
    temp->slots=response_item.slots_available;
    temp->state=ORTE_NODE_STATE_UP;

    opal_list_append(&nodes, &temp->super);
    
    cmd_received=(response_item.more_items==0? BBQ_CMD_FINISHED : BBQ_CMD_NODES_REPLY);
    return ORTE_SUCCESS;
}

static void launch_job(void)
{                    
    /*Insert received nodes into orte list for this job*/
    orte_ras_base_node_insert(&nodes, received_job);

    opal_output_verbose(0, orte_ras_base_framework.framework_output,
                "%s ras:bbq: Job allocation complete.",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    OBJ_DESTRUCT(&nodes);

    /* default to no-oversubscribe-allowed for managed systems */
    /* so ompi never allocates more process than slots available. */
    if (!(ORTE_MAPPING_SUBSCRIBE_GIVEN & ORTE_GET_MAPPING_DIRECTIVE(orte_rmaps_base.mapping))) {
        ORTE_SET_MAPPING_DIRECTIVE(orte_rmaps_base.mapping, ORTE_MAPPING_NO_OVERSUBSCRIBE);
    }
    /* flag that the allocation is managed (do not search in hostfile, take the ip from ras) */
    orte_managed_allocation = true;

    ORTE_ACTIVATE_JOB_STATE(received_job, ORTE_JOB_STATE_ALLOCATION_COMPLETE);
}

static int recv_cmd(void){
    int bytes;
    local_bbq_cmd_t response_cmd;
    
    bytes=read(socket_fd,&response_cmd,sizeof(local_bbq_cmd_t));
    if(bytes!=sizeof(local_bbq_cmd_t))
    {
        opal_output_verbose(0, orte_ras_base_framework.framework_output,
                "%s ras:bbq: Error while reading command.",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return ORTE_ERROR;
    }

    switch(response_cmd.cmd_type)
    {
        case BBQ_CMD_NODES_REPLY:
        {
            opal_output_verbose(0, orte_ras_base_framework.framework_output,
                "%s ras:bbq: BBQ sent command:BBQ_CMD_NODES_REPLY. Expecting node data. ",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            cmd_received=BBQ_CMD_NODES_REPLY;
            break;
        }
        default:
        {
            opal_output_verbose(0, orte_ras_base_framework.framework_output,
                "%s ras:bbq:error: BBQ sent command:unknown.",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            return ORTE_ERROR;
        }
    }
    return ORTE_SUCCESS;
}

static int send_cmd_node_request(void)
{
    local_bbq_job_t job;
    local_bbq_cmd_t command;
    orte_app_context_t *app;
    int i;
    
    command.cmd_type=BBQ_CMD_NODES_REQUEST;
    command.jobid=received_job->jobid;
    
    opal_output_verbose(0, orte_ras_base_framework.framework_output,
                    "%s ras:bbq: Sending command BBQ_CMD_NODES_REQUEST.",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

   
    if(0>write(socket_fd,&command,sizeof(local_bbq_cmd_t)))
    {
        opal_output_verbose(0, orte_ras_base_framework.framework_output,
            "%s ras:bbq:error: Error occurred while sending command BBQ_CMD_NODES_REQUEST.",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return ORTE_ERROR;
    }

    job.jobid=received_job->jobid;
    
    /*
     * Sums up the num_procs parameters of all the elements in apps array of the job. 
     * To be done in order to tell BBQ how many nodes have been requested via -np option in mpirun command.
     */
    job.slots_requested = 0;
    for (i=0; i < received_job->apps->size; i++) {
        if (NULL == (app = (orte_app_context_t*)opal_pointer_array_get_item(received_job->apps, i))) {
            continue;
        }
        job.slots_requested+=app->num_procs;
    }

    if(0>write(socket_fd,&job,sizeof(local_bbq_job_t)))
    {
        opal_output_verbose(0, orte_ras_base_framework.framework_output,
            "%s ras:bbq:error: Error occurred while sending resources request for job %d",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),job.jobid);
        return ORTE_ERROR;
    }

    opal_output_verbose(0, orte_ras_base_framework.framework_output,
        "%s ras:bbq: Requested %u slots for job %u.",
        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),job.slots_requested,job.jobid);
    
    return ORTE_SUCCESS;
}


static int send_cmd_terminate(void)
{
    local_bbq_cmd_t command;
    
    command.cmd_type=BBQ_CMD_TERMINATE;
    command.jobid=received_job->jobid;
    
    opal_output_verbose(0, orte_ras_base_framework.framework_output,
                "%s ras:bbq: Sending command BBQ_CMD_TERMINATE.",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    if(0>write(socket_fd,&command,sizeof(local_bbq_cmd_t)))
    {
        opal_output_verbose(0, orte_ras_base_framework.framework_output,
            "%s ras:bbq:error: Error occurred while sending command BBQ_CMD_TERMINATE.",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return ORTE_ERROR;
    }
    
    return ORTE_SUCCESS;
}
