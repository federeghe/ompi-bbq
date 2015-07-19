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
static int send_cmd(orte_job_t *jdata, int cmd);
static int recv_nodes_reply(local_bbq_res_item_t *response_host, opal_list_t *nodes);


/*
 * Global variable
 */
orte_ras_base_module_t orte_ras_bbq_module = {
    init,
    orte_ras_bbq_allocate,
    NULL,
    finalize
};

static int cmd_received=BBQ_CMD_NONE;
static int socket_fd;
static opal_event_t recv_ev;
static orte_job_t *received_job;

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
    
    socket_fd=socket(AF_INET,SOCK_STREAM,0);
    
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
    
    printf("bbq:module:Module initialized\n");
    
    return ORTE_SUCCESS;
}

static int recv_data(int fd, short args, void *cbdata)
{
    local_bbq_cmd_t response_cmd;
    local_bbq_res_item_t response_host;
    opal_list_t nodes;
    int bytes;
    
    OBJ_CONSTRUCT(&nodes, opal_list_t);
    
    printf("bbq:module:Data received!\n");
    
    while(true){
            
            //TODO: fault tolerance, check for message size
            switch(cmd_received){
                case BBQ_CMD_NONE:
                {
                    bytes=read(socket_fd,&response_cmd,sizeof(local_bbq_cmd_t));
                    if(bytes!=sizeof(local_bbq_cmd_t))
                    {
                        printf("bbq:module:Error while reading command\n");
                        return ORTE_ERROR;
                    }
                
                    switch(response_cmd.cmd_type)
                    {
                        case BBQ_CMD_NODES_REPLY:
                        {
                            printf("bbq:module:Received command BBQ_CMD_NODES_REPLY\n");
                            cmd_received=BBQ_CMD_NODES_REPLY;
                            break;
                        }
                        default:
                        {
                            printf("bbq:module:Unknown command received\n");
                            return ORTE_ERROR;
                        }
                    }
                    break;
                }
                case BBQ_CMD_NODES_REPLY:
                {
                    printf("bbq:module:Waiting for some data...\n");
                    bytes=read(socket_fd,&response_host,sizeof(local_bbq_res_item_t));
                    if(bytes!=sizeof(local_bbq_res_item_t))
                    {
                        printf("bbq:module:Error while reading host\n");
                        return ORTE_ERROR;
                    }
                    cmd_received=recv_nodes_reply(&response_host, &nodes);
                    break;
                }
                case BBQ_CMD_FINISHED:
                {
                    cmd_received=BBQ_CMD_NONE;
                    
                    orte_ras_base_node_insert(&nodes, received_job);
                
                    printf("bbq:module:Job allocation complete!\n");
                
                    OBJ_DESTRUCT(&nodes);
                
                    ORTE_ACTIVATE_JOB_STATE(received_job, ORTE_JOB_STATE_ALLOCATION_COMPLETE);
                    return ORTE_SUCCESS;
                }
                default:
                {
                    printf("bbq:module:Invalid cmd_received state");
                    return ORTE_ERROR;
                }
            }
            
        }
}

static int orte_ras_bbq_allocate(orte_job_t *jdata, opal_list_t *nodes)
{
    received_job=jdata;
    
    send_cmd(jdata, BBQ_CMD_NODES_REQUEST);
    
    return ORTE_ERR_ALLOCATION_PENDING;
}

static int send_cmd(orte_job_t *jdata, int cmd)
{
    local_bbq_cmd_t command;
    local_bbq_job_t job;
    
    command.cmd_type=cmd;
    
    switch(cmd){
        case BBQ_CMD_NODES_REQUEST:
        {
            printf("bbq:module:Sending command BBQ_CMD_NODES_REQUEST...\n");

            if(0>write(socket_fd,&command,sizeof(local_bbq_cmd_t)))
            {
                printf("bbq:module:Error while sending command\n");
                return ORTE_ERROR;
            }

            job.jobid=jdata->jobid;
            job.slots_requested=jdata->num_procs;


            if(0>write(socket_fd,&job,sizeof(local_bbq_job_t)))
            {
                printf("bbq:module:Error while sending job\n");
                return ORTE_ERROR;
            }
            
            printf("bbq:module:Asked resources for job %u, waiting for BBQ to answer...\n",
                    jdata->jobid);

            break;
        }
        case BBQ_CMD_TERMINATE:
        {
            printf("bbq:module:Sending command BBQ_CMD_TERMINATE...\n");
            if(0>write(socket_fd,&command,sizeof(local_bbq_cmd_t)))
            {
                printf("bbq:module:Error while sending command\n");
                return ORTE_ERROR;
            }
            break;
        }
    }
    return ORTE_SUCCESS;
}

static int finalize(void)
{
    send_cmd(NULL, BBQ_CMD_TERMINATE);
    
    opal_event_del(&recv_ev);
    
    OBJ_DESTRUCT(received_job);
    
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

static int recv_nodes_reply(local_bbq_res_item_t *response_host, opal_list_t *nodes)
{
    orte_node_t *temp;
    
    printf("bbq:module:Node data received\n");
    temp = OBJ_NEW(orte_node_t);

    temp->name=strdup(response_host->hostname);
    temp->slots_inuse=0;
    temp->slots_max=0;
    temp->slots=response_host->slots_available;
    temp->state=ORTE_NODE_STATE_UP;

    opal_list_append(nodes, &temp->super);
    printf("bbq:module:Node %s appended to the list with %d slots\n",temp->name,temp->slots);
    printf("bbq:module:Nodes left to append: %d\n", response_host->more_items);
    
    return response_host->more_items==0? BBQ_CMD_FINISHED : BBQ_CMD_NODES_REPLY;
}