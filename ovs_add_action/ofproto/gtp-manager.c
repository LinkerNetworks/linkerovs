#include <config.h>

#include "gtp-manager.h"
#include "openvswitch/vlog.h"

VLOG_DEFINE_THIS_MODULE(gtp_manager);

static struct ovs_mutex mutex;

static uint16 ovs_id OVS_GUARDED_BY(mutex);
static uint16 ovs_total OVS_GUARDED_BY(mutex);
static uint16 ovs_phy_port OVS_GUARDED_BY(mutex);
static uint8 pgw_fastpath OVS_GUARDED_BY(mutex);

static struct gtp_pgw_node pgw_list[MAX_PGW_PER_NODE];
static uint16 current;
static uint16 end;

static struct ovs_mutex teid2pgw_mutex[HASHMAP_PART_NUM];
static struct cmap teid2pgw[HASHMAP_PART_NUM];

static struct ovs_mutex ueip2pgw_mutex[HASHMAP_PART_NUM];
static struct cmap ueip2pgw[HASHMAP_PART_NUM];

void
gtp_manager_init(void)
{
    int i;
    
    ovs_mutex_init(&mutex);
    ovs_mutex_lock(&mutex);
    current = 0;
    end = 0;

    int i = 0;
    for (i = 0; i < MAX_PGW_PER_NODE; ++i)
    {
        pgw_list[i].gtp_pgw_ip = 0;
    }
    for (i = 0; i < HASHMAP_PART_NUM; i++) {
        cmap_init(&teid2pgw[i]);
    }
    ovs_mutex_unlock(&mutex);
}

void
gtp_manager_set_params(uint16 ovsid, uint16 total, uint16 phyport, uint8 fastpath)
{
    ovs_mutex_lock(&mutex);
    ovs_id = ovsid;
    ovs_total = total;
    ovs_phy_port = phyport;
    pgw_fastpath = fastpath;
    ovs_mutex_unlock(&mutex);
}

void
gtp_manager_add_pgw(ovs_be32 gtp_pgw_ip, uint16 gtp_pgw_port, struct eth_addr gtp_pgw_eth, uint16 pgw_sgi_port, struct eth_addr pgw_sgi_eth) 
{
    bool found = false;
    int i = 0;
    for (i = 0; i < end; ++i)
    {
        if (pgw_list[i].gtp_pgw_ip == 0)
        {
            pgw_list[i].gtp_pgw_ip = gtp_pgw_ip;
            pgw_list[i].gtp_pgw_port = gtp_pgw_port;
            pgw_list[i].gtp_pgw_eth = gtp_pgw_eth;
            pgw_list[i].pgw_sgi_port = pgw_sgi_port;
            pgw_list[i].pgw_sgi_eth = pgw_sgi_eth;
            found = true;
            break;
        }
    }
    if (!found)
    {
        ASSERT(end < MAX_PGW_PER_NODE);
        i = end++;
        pgw_list[i].gtp_pgw_ip = gtp_pgw_ip;
        pgw_list[i].gtp_pgw_port = gtp_pgw_port;
        pgw_list[i].gtp_pgw_eth = gtp_pgw_eth;
        pgw_list[i].pgw_sgi_port = pgw_sgi_port;
        pgw_list[i].pgw_sgi_eth = pgw_sgi_eth;
    }
}

void 
gtp_manager_del_pgw(ovs_be32 gtp_pgw_ip)
{
    int i = 0;
    for (i = 0; i < end; ++i)
    {
        if (pgw_list[i].gtp_pgw_ip==gtp_pgw_ip)
        {
            pgw_list[i].gtp_pgw_ip=0;
            pgw_list[i].gtp_pgw_port=0;
            pgw_list[i].gtp_pgw_eth=0;
            pgw_list[i].pgw_sgi_port=0;
            pgw_list[i].pgw_sgi_eth=0;
            break;
        }
    }
}

int
gtp_manager_get_pgw(void)
{
    struct gtp_pgw_node * pnode = NULL;
    int index = -1;
    int i;
    for (i = current; i < end; ++i)
    {
        if (pgw_list[i].gtp_pgw_ip != 0)
        {
            index = i;
            current = i + 1;
            break;
        }
    }
    if (index == -1)
    {
        index = 0;
        current = 1;
    }

    return index;
}

int gtp_manager_find_pgw(ovs_be32 gtp_pgw_ip){
    int foundindex = -1;
    int i;
    for (i = 0; i < end; ++i)
    {
        if (pgw_list[0].gtp_pgw_ip == gtp_pgw_ip)
        {
            foundindex = i;
            break;
        }
    }
    if (foundindex == -1)
    {
        return -1;
    }
}

int
gtp_manager_put_teid_pgw(uint32 teid, struct gtp_tunnel_node * gtp_tunnel_node)
{
    uint32 cmap_id = teid & 0x0000001F;
    cmap teid2pgwmap = teid2pgw[cmap_id];
    struct gtp_teid_to_pgw_node * node = xmalloc(sizeof *node);
    node->teid = teid;
    node->gtp_tunnel_node = gtp_tunnel_node;
    node->ref_count = 1;
    ovs_mutex_lock(&teid2pgw_mutex[cmap_id]);
    size_t ret = cmap_insert(&teid2pgwmap, CONST_CAST(struct cmap_node *, &node->node),
                hash_int(teid, 0));
    ovs_mutex_unlock(&teid2pgw_mutex[cmap_id]);
    return ret;
}

struct gtp_teid_to_pgw_node * 
gtp_manager_get_teid_pgw(uint32 teid)
{
    uint32 cmap_id = teid & 0x0000001F;
    cmap teid2pgwmap = teid2pgw[cmap_id]; 
    struct gtp_teid_to_pgw_node *pgw_node;

    CMAP_FOR_EACH_WITH_HASH (pgw_node, node, hash_int(teid, 0), &teid2pgwmap) {
        if (pgw_node->teid == teid) {
            ovs_mutex_lock(&pgw_node->mutex);
            pgw_node->ref_count++;
            ovs_mutex_unlock(&pgw_node->mutex);
            return pgw_node;
        }
    }
    return NULL;
}

int gtp_manager_del_teid_pgw(uint32 teid)
{
    uint32 cmap_id = teid & 0x0000001F;
    cmap teid2pgwmap = teid2pgw[cmap_id];
    uint32_t hash = hash_int(teid, 0);
    struct gtp_teid_to_pgw_node * pgw_node;
    struct gtp_teid_to_pgw_node * found_node;
    CMAP_FOR_EACH_WITH_HASH (pgw_node, node, hash, &teid2pgwmap) {
        if (pgw_node->teid == teid) {
            found_node = pgw_node;
            break;
        }
    }
    if (!found_node)
    {
        return -1;
    }
    ovs_mutex_lock(&teid2pgw_mutex[cmap_id]);
    size_t ret = cmap_remove(&teid2pgwmap, found_node->node, hash);
    ovs_mutex_unlock(&teid2pgw_mutex[cmap_id]);

    bool willfree = false;
    ovs_mutex_lock(&found_node->mutex);
    found_node->ref_count--;
    if (found_node->ref_count == 0) {
        willfree = true;
    }
    ovs_mutex_unlock(&found_node->mutex);
    if (willfree)
    {
        //just unref, do not free, let t2 to free it
        found_node->gtp_tunnel_node->ref_count--;
        if (found_node->gtp_tunnel_node->ref_count==0)
        {
            free(found_node->gtp_tunnel_node);
        }
        free(found_node);
    }

    return ret;
}

int
gtp_manager_put_ueip_pgw(ovs_be32 ueip, struct gtp_tunnel_node * gtp_tunnel_node)
{
    uint32 cmap_id = (ueip & 0x1F000000) >> 24;
    cmap ueip2pgwmap = ueip2pgw[cmap_id];
    struct gtp_ueip_to_pgw_node * node = xmalloc(sizeof *node);
    node->ueip = ueip;
    node->gtp_tunnel_node = gtp_tunnel_node;
    node->ref_count = 1;
    ovs_mutex_lock(&ueip2pgw_mutex[cmap_id]);
    size_t ret = cmap_insert(&ueip2pgwmap, CONST_CAST(struct cmap_node *, &node->node),
                hash_int(ueip, 0));
    ovs_mutex_unlock(&ueip2pgw_mutex[cmap_id]);
    return ret;
}

struct gtp_ueip_to_pgw_node * 
gtp_manager_get_ueip_pgw(uint32 ueip)
{
    uint32 cmap_id = (ueip & 0x1F000000) >> 24;
    cmap ueip2pgwmap = ueip2pgw[cmap_id]; 
    struct gtp_ueip_to_pgw_node *pgw_node;

    CMAP_FOR_EACH_WITH_HASH (pgw_node, node, hash_int(ueip, 0), &ueip2pgwmap) {
        if (pgw_node->ueip == ueip) {
            ovs_mutex_lock(&pgw_node->mutex);
            pgw_node->ref_count++;
            ovs_mutex_unlock(&pgw_node->mutex);
            return pgw_node;
        }
    }
    return NULL;
}

int gtp_manager_del_ueip_pgw(uint32 ueip)
{
    uint32 cmap_id = (ueip & 0x1F000000) >> 24;
    cmap ueip2pgwmap = ueip2pgw[cmap_id];
    uint32_t hash = hash_int(ueip, 0);
    struct gtp_ueip_to_pgw_node * pgw_node;
    struct gtp_ueip_to_pgw_node * found_node;
    CMAP_FOR_EACH_WITH_HASH (pgw_node, node, hash, &ueip2pgwmap) {
        if (pgw_node->ueip == ueip) {
            found_node = pgw_node;
            break;
        }
    }
    if (!found_node)
    {
        return -1;
    }
    ovs_mutex_lock(&ueip2pgw_mutex[cmap_id]);
    size_t ret = cmap_remove(&ueip2pgwmap, found_node->node, hash);
    ovs_mutex_unlock(&ueip2pgw_mutex[cmap_id]);
    
    bool willfree = false;
    ovs_mutex_lock(&found_node->mutex);
    found_node->ref_count--;
    if (found_node->ref_count == 0) {
        willfree = true;
    }
    ovs_mutex_unlock(&found_node->mutex);
    if (willfree)
    {
        //just unref, do not free, let t2 to free it
        found_node->gtp_tunnel_node->ref_count--;
        if (found_node->gtp_tunnel_node->ref_count==0)
        {
            free(found_node->gtp_tunnel_node);
        }
        free(found_node);
    }

    return ret;
}

bool maybe_gtpc_message(struct flow *flow){
    if (is_ip_any(flow) && flow->nw_proto == IPPROTO_UDP && flow->tp_dst ==htons(GTPC_PORT) && flow->tp_src == htons(GTPC_PORT) ) {
        return true;
    }
    return false;
}

bool maybe_gtpu_message(struct flow *flow){
    if (is_ip_any(flow) && flow->nw_proto == IPPROTO_UDP && flow->tp_dst ==htons(GTPU_PORT) && flow->tp_src == htons(GTPU_PORT) ) {
        return true;
    }
    return false;
} 

struct gtpc_msg_header *
parse_gtpc_msg_header(struct flow *flow, struct dp_packet * packet)
{
    struct gtpc_msg_header * msg = xmalloc(sizeof *msg);
    int offset = 0;
    uint8 * flag = dp_packet_at(packet, offset, 1);
    msg->version = (*flag & 0xe0) >> 5;
    if(msg->version != 2){
        return NULL;
    }
    msg->has_teid = (*flag & 0x08) == 0 ? false:true;
    offset = offset + 1;
    uint8 * message_type = dp_packet_at(packet, offset, 1);
    msg->message_type = *message_type;
    offset = offset + 1;
    uint16 * message_length = dp_packet_at(packet, offset, 2);
    msg->message_length= *message_length;
    offset = offset + 2;
    if (msg->has_teid)
    {
        uint32 * teid = dp_packet_at(packet, offset, 4);
        msg->teid = *teid;
        offset = offset + 4;
    }
    uint32 * seq_num = dp_packet_at(packet, offset, 4);
    msg->seq_num = (*seq_num) >> 8;
    offset = offset + 4;
    msg->body_offset = offset;   
    return msg;
}

struct gtpu_msg_header *
parse_gtpu_message(struct flow *flow, struct dp_packet * packet)
{
    struct gtpu_msg_header * msg = xmalloc(sizeof *msg);
    int offset = 0;
    uint8 * flag = dp_packet_at(packet, offset, 1);
    msg->version = (*flag & 0xe0) >> 5;
    if(msg->version != 1){
        free(msg);
        return NULL;
    }
    offset = offset + 1;
    uint8 * message_type = dp_packet_at(packet, offset, 1);
    msg->message_type = *message_type;
    offset = offset + 1;
    uint16 * message_length = dp_packet_at(packet, offset, 2);
    msg->message_length= *message_length;
    offset = offset + 2;
    uint32 * teid = dp_packet_at(packet, offset, 4);
    msg->teid = *teid;
    offset = offset + 4;
    msg->body_offset = offset;
    return msg;
}

void
handle_gtpc_message(struct flow *flow, struct flow_wildcards *wc, struct gtpc_msg_header * gtpcmsg, struct dp_packet * packet, struct xlate_ctx *ctx)
{
    if (flow->in_port.ofp_port == ovs_phy_port){
        //from sgw to pgw
        if (!gtpcmsg->has_teid || gtpcmsg->teid == 0)
        {
            if (gtpcmsg->seq_num%ovs_total == ovs_id)
            {
                int ramdompgw = gtp_manager_get_pgw();
                if (gtpcmsg->message_type == 32)
                {
                    //create session request
                    if (pgw_fastpath != 0)
                    {
                        //enable fast path
                        //parse body to get t2, t3
                        //create gtp_tunnel_node, set t2, t3, pgwindex
                        //put t2->pnode, t3->pnode, ref_count=2
                    }
                }

                //set dst to pgw ip and mac
                memset(&wc->masks.nw_dst, 0xff, sizeof wc->masks.nw_dst);
                flow->nw_dst = pgw_lit[ramdompgw].gtp_pgw_ip;
                WC_MASK_FIELD(wc, dl_dst);
                flow->dl_dst = pgw_lit[ramdompgw].gtp_pgw_eth;
                //output to pgw port
                xlate_output_action(ctx, pgw_list[ramdompgw].gtp_pgw_port, 0, false);   
            }
            //else drop
        } else {
            struct gtp_teid_to_pgw_node * selectedpgw = gtp_manager_get_teid_pgw(gtpcmsg->teid);
            if (selectedpgw == NULL)
            {
                //drop
            } else {
                if (pgw_list[selectedpgw->gtp_tunnel_node->pgw_index].gtp_pgw_ip == 0) {
                    //drop
                } else {
                    //set dst to pgw ip and mac
                    memset(&wc->masks.nw_dst, 0xff, sizeof wc->masks.nw_dst);
                    flow->nw_dst = pgw_lit[selectedpgw].gtp_pgw_ip;
                    WC_MASK_FIELD(wc, dl_dst);
                    flow->dl_dst = pgw_lit[selectedpgw].gtp_pgw_eth;
                    //output to pgw port
                    xlate_output_action(ctx, pgw_list[selectedpgw].gtp_pgw_port, 0, false);
                }
                bool willfree = false;
                ovs_mutex_lock(&selectedpgw->mutex);
                selectedpgw->ref_count--;
                if (selectedpgw->ref_count == 0) {
                    willfree = true;
                }
                ovs_mutex_unlock(&selectedpgw->mutex);
                if (willfree)
                {
                    //just unref, do not free, let t2 to free it
                    selectedpgw->gtp_tunnel_node->ref_count--;
                    if (selectedpgw->gtp_tunnel_node->ref_count==0)
                    {
                        free(selectedpgw->gtp_tunnel_node);
                    }
                    free(selectedpgw);
                }
            }
        } 
        
    } else {
        //from pgw to sgw
        if (!gtpcmsg->has_teid || gtpcmsg->teid == 0) {
            //output to phy port
            xlate_output_action(ctx, ovs_phy_port, 0, false);
        } else if (gtpcmsg->message_type == 33){
            //create session response
            //int ret = gtp_manager_put_teid_pgw(gtpcmsg->teid, flow->nw_src);
            //get t2 from header
            uint32 t2 = gtpcmsg->teid;
            //continue to parse body
            int start = gtpcmsg->body_offset;
            //Octets 3 to 4 represent the Message Length field. 
            //This field shall indicate the length of the message in octets excluding the mandatory part of the GTP-C header (the first 4 octets).
            int length = gtpcmsg->message_length + 4;
            //get t4, t5, ueip
            uint32 t4 = 0;
            uint32 t5 = 0;
            ovs_be32 ueip = 0;
            while(start < length){
                uint8 * ie_type = dp_packet_at(packet, start, 1);
                uint16 * ie_length = dp_packet_at(packet, start+1, 2);
                if (*ie_type == 87)
                {
                    //t4
                    uint32 *t4p = dp_packet_at(packet, start+5, 4);
                    t4 = *t4p;
                } else if (*ie_type == 79)
                {
                    //ueip
                    uint32 *ueipp = dp_packet_at(packet, start+5, 4);
                    ueip = htonl(*ueipp);
                } else if (*ie_type == 93)
                {
                    //bearer context
                    int bcstart = start+4;
                    int bcstartend = bcstart + *ie_length;
                    while(bcstart < bcstartend){
                        uint8 * bc_ie_type = dp_packet_at(packet, bcstart, 1);
                        uint16 * bc_ie_length = dp_packet_at(packet, bcstart+1, 2);
                        if (*bc_ie_type == 87)
                        {
                            uint32 *t5p = dp_packet_at(packet, bcstart+5, 4);
                            t5 = *t5p;
                            break;
                        }
                        bcstart = bcstart + *bc_ie_length + 4;
                    }
                }
                if (t4 !=0 && t5 !=0 && ueip != 0)
                {
                    break;
                }
                start = start + *ie_length + 4;
            }
            if (t4 !=0 && t5 !=0 && ueip != 0) {
                if (pgw_fastpath == 0)
                {
                    //not fast path
                    ////create gtp_tunnel_node,
                    struct gtp_tunnel_node * node = xmalloc(sizeof *node);
                    ////set t4, t5, ueip, pgwindex, also set t2 for delete response
                    node->teid2_pgw_c=t2;
                    node->teid4_sgw_c=t4;
                    node->teid5_sgw_u=t5;
                    node->ue_ip = ueip;

                    ////put t4->pnode, t5->pnode, ueip->pnode, t2->pnode
                    int pgwindex = gtp_manager_find_pgw(flow->nw_src);
                    if (pgwindex != -1)
                    {
                        node->pgw_index = pgwindex;
                        node->ref_count = 4;
                        gtp_manager_put_teid_pgw(t4, node);
                        gtp_manager_put_teid_pgw(t5, node);
                        gtp_manager_put_ueip_pgw(ueip, node);
                        gtp_manager_put_teid_pgw(t2, node);
                        ////refcount = refcount+4
                        //output to phy port
                        xlate_output_action(ctx, ovs_phy_port, 0, false);
                    }

                } else {
                    //fast path
                    ////get gtp_tunnel_node with t2 
                    ////and check gtp_tunnel_node's pgw==the current pgw
                    ////set t4, t5, ueip
                    ////put t4->pnode, t5->pnode, ueip->pnode
                    ////refcount = refcount+3
                    //output to phy port
                }
            }
            //drop
        } else if (gtpcmsg->message_type == 37) {
            //delete session response
            //int ret = gtp_manager_del_teid_pgw(gtpcmsg->teid);
            //teid in header is t2
            uint32 t2 = gtpcmsg->teid;
            //both fastpath and not fastpath, the t2 is in map, get gtp_tunnel_node from t2
            struct gtp_teid_to_pgw_node * node = gtp_manager_get_teid_pgw(t2);
            uint32 t4 = node->gtp_tunnel_node->teid4_sgw_c;
            uint32 t5 = node->gtp_tunnel_node->teid5_sgw_u;
            uint32 ueip = node->gtp_tunnel_node->ue_ip;
            //delete t2, t3 when fastpath, t4, t5, ueip in map, delete one, unref one, if zero, free the node
            gtp_manager_del_teid_pgw(t4);
            gtp_manager_del_teid_pgw(t5);
            gtp_manager_del_ueip_pgw(ueip);
            gtp_manager_del_teid_pgw(t2);

            bool willfree = false;
            ovs_mutex_lock(&node->mutex);
            node->ref_count--;
            if (node->ref_count == 0) {
                willfree = true;
            }
            ovs_mutex_unlock(&node->mutex);
            if (willfree)
            {
                //just unref, do not free, let t2 to free it
                node->gtp_tunnel_node->ref_count--;
                if (node->gtp_tunnel_node->ref_count==0)
                {
                    free(node->gtp_tunnel_node);
                }
                free(node);
            }
            //output to phy port
            xlate_output_action(ctx, ovs_phy_port, 0, false);
        } else {
            //output to phy port
            xlate_output_action(ctx, ovs_phy_port, 0, false);
        } 
    }
}

void
handle_gtpu_message(struct flow *flow, struct flow_wildcards *wc, struct gtpu_msg_header * gtpumsg, struct dp_packet * packet, struct xlate_ctx *ctx)
{
    if (flow->in_port.ofp_port == ovs_phy_port){
        //from sgw to pgw
        if (gtpumsg->teid == 0){
            //drop
        } else {
            if (pgw_fastpath == 0){
                //not fastpath
                //teid in header is t5
                //get gtp_tunnel_node with t5 from map
                //get pgw
                //output to pgw port
                struct gtp_teid_to_pgw_node * selectedpgw = gtp_manager_get_teid_pgw(gtpumsg->teid);
                if (selectedpgw == NULL)
                {
                    //drop
                } else {
                    if (pgw_list[selectedpgw->gtp_tunnel_node->pgw_index].gtp_pgw_ip == 0) {
                        //drop
                    } else {
                        //set dst to pgw ip and mac
                        memset(&wc->masks.nw_dst, 0xff, sizeof wc->masks.nw_dst);
                        flow->nw_dst = pgw_lit[selectedpgw].gtp_pgw_ip;
                        WC_MASK_FIELD(wc, dl_dst);
                        flow->dl_dst = pgw_lit[selectedpgw].gtp_pgw_eth;
                        //output to pgw port
                        xlate_output_action(ctx, pgw_list[selectedpgw].gtp_pgw_port, 0, false);
                    }
                    bool willfree = false;
                    ovs_mutex_lock(&selectedpgw->mutex);
                    selectedpgw->ref_count--;
                    if (selectedpgw->ref_count == 0) {
                        willfree = true;
                    }
                    ovs_mutex_unlock(&selectedpgw->mutex);
                    if (willfree)
                    {
                        //just unref, do not free, let t2 to free it
                        selectedpgw->gtp_tunnel_node->ref_count--;
                        if (selectedpgw->gtp_tunnel_node->ref_count==0)
                        {
                            free(selectedpgw->gtp_tunnel_node);
                        }
                        free(selectedpgw);
                    }
                }                
            } else {
                //get body from package
                //send the ip msg in body to phy port
                //how? modify the current? drop the current and create new one?
                //learn from arp
            }
        }
    } else {
        //from pgw to sgw
        if (pgw_fastpath == 0)
        {
            //not fast path
            //output to phy port
            xlate_output_action(ctx, ovs_phy_port, 0, false);
        } else {
            //should not be here
        }
    }
}

void handle_gtp(struct flow *flow, struct flow_wildcards *wc, struct dp_packet * packet, struct xlate_ctx *ctx)
{
    if (maybe_gtpc_message(flow)){
        struct gtpc_msg_header * gtpcmsgheader = NULL;
        gtpcmsgheader = parse_gtpc_msg_header(flow, packet);
        if(gtpcmsgheader != NULL) {
            handle_gtpc_message(flow, wc, gtpcmsgheader, packet, ctx);
                free(gtpcmsgheader);                    
            }
        } else if (maybe_gtpu_message(flow)){
            struct gtpu_msg_header * gtpumsgheader = NULL;
            gtpumsgheader = parse_gtpu_message(flow, packet);
            if(gtpumsgheader != NULL) {
                handle_gtpu_message(flow, wc, gtpumsgheader, packet, ctx);
                free(gtpumsgheader);                    
            }
        }
    }
}

//src_ip=ueip_pool or dst_ip=ueip_pool, should be handled here
void handle_pgw_sgi(struct flow *flow, struct flow_wildcards *wc, struct dp_packet * packet, struct xlate_ctx *ctx)
{
    if (flow->in_port.ofp_port == ovs_phy_port){
        //from www to pgw
        if (pgw_fastpath == 0)
        {
            //not fast path
            //dst=ueip
            //get pgw with ueip, if no drop
            //send packet to pgw sgi port, do not change packet
            struct gtp_ueip_to_pgw_node * selectedpgw = gtp_manager_put_ueip_pgw(flow->nw_dst);
            if (selectedpgw == NULL)
            {
                //drop
            } else {
                if (pgw_list[selectedpgw->gtp_tunnel_node->pgw_index].gtp_pgw_ip == 0) {
                    //drop
                } else {
                    WC_MASK_FIELD(wc, dl_dst);
                    flow->dl_dst = pgw_lit[selectedpgw].pgw_sgi_eth;
                    //output to pgw port
                    xlate_output_action(ctx, pgw_list[selectedpgw].pgw_sgi_port, 0, false);
                }
                bool willfree = false;
                ovs_mutex_lock(&selectedpgw->mutex);
                selectedpgw->ref_count--;
                if (selectedpgw->ref_count == 0) {
                    willfree = true;
                }
                ovs_mutex_unlock(&selectedpgw->mutex);
                if (willfree)
                {
                    //just unref, do not free, let t2 to free it
                    selectedpgw->gtp_tunnel_node->ref_count--;
                    if (selectedpgw->gtp_tunnel_node->ref_count==0)
                    {
                        free(selectedpgw->gtp_tunnel_node);
                    }
                    free(selectedpgw);
                }
            }       
        } else {
            //fast path
            //get t3 with ueip
            //create new package with t3 and send to phy port
        }

    } else {
        //from pgw to www
        if (pgw_fastpath == 0)
        {
            //not fastpath
            //output to phy port
            xlate_output_action(ctx, ovs_phy_port, 0, false);
        } else {
            //fast path
            //should not be here
            //handle gtpu should send the package directly
        }
    }
}

