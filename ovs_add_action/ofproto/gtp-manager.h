#ifndef GTP_MANAGER_H
#define GTP_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#include "cmap.h"
#include "list.h"
#include "ovs-thread.h"
#include "ofproto/ofproto-dpif-xlate.h"

#define HASHMAP_PART_NUM 32
#define MAX_PGW_PER_NODE 1024
#define GTPU_PORT 2152
#define GTPC_PORT 2123

struct gtp_pgw_node {
	uint16_t gtp_pgw_port;
	struct eth_addr gtp_pgw_eth; 
	uint16_t pgw_sgi_port;
	struct eth_addr pgw_sgi_eth; 
    ovs_be32 gtp_pgw_ip;
};

struct gtp_teid_to_pgw_node {
	struct cmap_node node;
	uint32_t teid; 
	struct gtp_tunnel_node * gtp_tunnel_node;
	struct ovs_mutex mutex;
	uint8_t ref_count;
};

struct gtp_ueip_to_pgw_node {
	struct cmap_node node;
	ovs_be32 ueip; 
	struct gtp_tunnel_node * gtp_tunnel_node;
	struct ovs_mutex mutex;
	uint8_t ref_count;
};

struct gtp_tunnel_node
{
	uint32_t teid4_sgw_c;
	uint32_t teid5_sgw_u;
	uint32_t teid2_pgw_c;
	uint32_t teid3_pgw_u;
	ovs_be32 ue_ip;
	uint16_t pgw_index;
	int ref_count;
};

struct gtpu_msg_header {
	uint8_t version;
	uint8_t message_type;
	uint16_t message_length;
	uint32_t teid;

	int body_offset;
};

struct gtpc_msg_header {
	uint8_t version;
	bool has_teid;
	uint8_t message_type;
	uint16_t message_length;
	uint32_t teid;
	uint32_t seq_num;

	int body_offset;
};

void gtp_manager_init(void);
void gtp_manager_dump(void);
void gtp_manager_set_params(uint16_t ovs_id, uint16_t total, uint16_t phyport, uint8_t fastpath);
void gtp_manager_add_pgw(ovs_be32 gtp_pgw_ip, uint16_t gtp_pgw_port, struct eth_addr gtp_pgw_eth, uint16_t pgw_sgi_port, struct eth_addr pgw_sgi_eth);
void gtp_manager_del_pgw(ovs_be32 gtp_pgw_ip);
int gtp_manager_get_pgw(void);
int gtp_manager_find_pgw(ovs_be32 gtp_pgw_ip);

int gtp_manager_put_teid_pgw(uint32_t teid, struct gtp_tunnel_node * gtp_tunnel_node);
int gtp_manager_del_teid_pgw(uint32_t teid);
struct gtp_teid_to_pgw_node * gtp_manager_get_teid_pgw(uint32_t teid);

int gtp_manager_put_ueip_pgw(ovs_be32 ueip, struct gtp_tunnel_node * gtp_tunnel_node);
struct gtp_ueip_to_pgw_node * gtp_manager_get_ueip_pgw(uint32_t ueip);
int gtp_manager_del_ueip_pgw(uint32_t ueip);

bool maybe_gtpc_message(struct flow *flow);
bool maybe_gtpu_message(struct flow *flow);
struct gtpc_msg_header * parse_gtpc_msg_header(const struct dp_packet * packet);
struct gtpu_msg_header * parse_gtpu_message(const struct dp_packet * packet);

void handle_gtpc_message(struct flow *flow, struct flow_wildcards *wc, struct gtpc_msg_header * gtpcmsg, const struct dp_packet * packet, struct xlate_ctx *ctx);
void handle_gtpu_message(struct flow *flow, struct flow_wildcards *wc, struct gtpu_msg_header * gtpumsg, const struct dp_packet * packet, struct xlate_ctx *ctx);
void handle_gtp(struct flow *flow, struct flow_wildcards *wc, const struct dp_packet * packet, struct xlate_ctx *ctx);
void handle_pgw_sgi(struct flow *flow, struct flow_wildcards *wc, const struct dp_packet * packet, struct xlate_ctx *ctx);
#endif
