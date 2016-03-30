#ifndef BBQ_TYPES
#define BBQ_TYPES

/* 
 * BBQ-MPI-related constants
 */

#define BBQ_CMD_NONE -1

/* Commands */
#define BBQ_CMD_NODES_REQUEST 0
#define BBQ_CMD_NODES_REPLY 1
#define BBQ_CMD_TERMINATE 2

/* MIG-related commands*/
#define BBQ_CMD_MIGRATE 3
#define BBQ_CMD_MIGRATION_READY 4
#define BBQ_CMD_MIGRATION_ONGOING 5
#define BBQ_CMD_MIGRATION_ABORTED 6
#define BBQ_CMD_MIGRATION_SUCCEEDED 7

/* Options */
#define BBQ_OPT_MIG_AVAILABLE 1 /* ras -> bbq */
#define BBQ_OPT_PREDUMP_REQ 2   /* bbq -> ras */

#include <stdint.h>

/***********  Common   ************/
struct local_bbq_cmd_t {
	uint32_t jobid;			/* The jobid  */
	uint8_t cmd_type;		/* Command number: */
    uint8_t flags;          /* Flags */
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

/***********  BBQ -> OpenMPI  ************/
struct local_bbq_migrate_t {
	uint32_t jobid;
	char src[256];                                  /* orte_node_t / source node */
    char dest[256];                                 /* orte_node_t / destination node */
};
typedef struct local_bbq_migrate_t local_bbq_migrate_t;

#endif
