#ifndef BBQ_TYPES
#define BBQ_TYPES

#define BBQ_CMD_FINISHED -2
#define BBQ_CMD_NONE -1
#define BBQ_CMD_NODES_REQUEST 0
#define BBQ_CMD_NODES_REPLY 1
#define BBQ_CMD_TERMINATE 2

#include <stdint.h>

/***********  Common   ************/
struct local_bbq_cmd_t {
	uint8_t cmd_type;		/* Command number: */
	uint32_t jobid;						/* -  */
							/* Other things here? */
};
typedef struct local_bbq_cmd_t local_bbq_cmd_t;

/***********  OpenMPI -> BBQ   ************/
struct local_bbq_job_t {
	uint32_t jobid;
	uint32_t slots_requested;
};
typedef struct local_bbq_job_t local_bbq_job_t;


/***********  BBQ -> OpenMPI  ************/
struct local_bbq_res_item_t {
	uint32_t jobid;
	char hostname[256];					/* orte_node_t / name */
	int32_t slots_available;			/* orte_node_t / slots */
	uint8_t more_items;					/* boolean flag */
};
typedef struct local_bbq_res_item_t local_bbq_res_item_t;

#endif
