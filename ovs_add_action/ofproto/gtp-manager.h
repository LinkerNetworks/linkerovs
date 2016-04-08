#ifndef GTP_MANAGER_H
#define GTP_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#include "cmap.h"
#include "list.h"
#include "ovs-thread.h"

#define HASHMAP_PART_NUM 32;
#define MAX_PGW_PER_NODE 1024;
#define GTPU_PORT 2152;
#define GTPC_PORT 2123;

struct gtp_pgw_node {
	uint16 gtp_pgw_port;
    ovs_be32 gtp_pgw_ip;
};

struct gtp_teid_to_pgw_node {
	struct cmap_node node;
	uint32 teid; 
	uint16 pgw_index;
};

struct gtpu_message {
	uint8 version;
	uint8 message_type;
	uint16 message_length;
	uint32 teid;
};

struct gtpc_message {
	uint8 version;
	bool has_teid;
	uint8 message_type;
	uint16 message_length;
	uint32 teid;
	uint32 seq_num;
};

void gtp_manager_init(void);
void gtp_manager_set_ovs_id(uint16 ovs_id, uint16 total, uint16 phyport);
void gtp_manager_add_pgw(ovs_be32 gtp_pgw_ip, uint16 gtp_pgw_port);
void gtp_manager_del_pgw(ovs_be32 gtp_pgw_ip);
struct gtp_pgw_node * gtp_manager_get_pgw(void);

int gtp_manager_put_teid_pgw(uint32 teid, ovs_be32 gtp_pgw_ip);
int gtp_manager_del_teid_pgw(uint32 teid);
struct gtp_pgw_node * gtp_manager_get_teid_pgw(uint32 teid);

#endif
