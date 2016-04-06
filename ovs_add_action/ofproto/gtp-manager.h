#ifndef GTP_MANAGER_H
#define GTP_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#include "cmap.h"
#include "list.h"
#include "ovs-thread.h"

#define HASHMAP_PART_NUM 32;
#define MAX_PGW_PER_NODE 1024;

struct gtp_pgw_node {
    ovs_be32 gtp_pgw_ip;
};

struct gtp_teid_to_pgw_node {
	struct cmap_node node;
	uint32 teid; 
	uint16 pgw_index;
};

void gtp_manager_init(void);
void gtp_manager_set_ovs_id(uint16 ovs_id, uint16 total);
void gtp_manager_add_pgw(ovs_be32 gtp_pgw_ip);
void gtp_manager_del_pgw(ovs_be32 gtp_pgw_ip);
ovs_be32 gtp_manager_get_pgw(void);

int gtp_manager_put_teid_pgw(uint32 teid, ovs_be32 gtp_pgw_ip);
int gtp_manager_del_teid_pgw(uint32 teid);
ovs_be32 gtp_manager_get_teid_pgw(uint32 teid);

#endif
