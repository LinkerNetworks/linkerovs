#include <config.h>

#include "lib/util.h"
#include "gtp-manager.h"
#include "openvswitch/vlog.h"

VLOG_DEFINE_THIS_MODULE(gtp_manager);

static struct ovs_mutex mutex;

static uint16_t ovs_id OVS_GUARDED_BY(mutex);
static uint16_t ovs_total OVS_GUARDED_BY(mutex);
static uint16_t ovs_phy_port OVS_GUARDED_BY(mutex);
static uint8_t pgw_fastpath OVS_GUARDED_BY(mutex);

static struct gtp_pgw_node pgw_list[MAX_PGW_PER_NODE];
static uint16_t current;
static uint16_t end;

static struct ovs_mutex teid2pgw_mutex[HASHMAP_PART_NUM];
static struct cmap teid2pgw[HASHMAP_PART_NUM];

static struct ovs_mutex ueip2pgw_mutex[HASHMAP_PART_NUM];
static struct cmap ueip2pgw[HASHMAP_PART_NUM];

bool
check_is_gtp(const struct flow * flow)
{
    return (maybe_gtpc_message(flow) || maybe_gtpu_message(flow));
}

void
gtp_manager_init(void)
{
    int i;
    
    ovs_mutex_init(&mutex);
    ovs_mutex_lock(&mutex);
    current = 0;
    end = 0;

    for (i = 0; i < MAX_PGW_PER_NODE; ++i)
    {
        pgw_list[i].gtp_pgw_ip = 0;
    }
    for (i = 0; i < HASHMAP_PART_NUM; i++) {
        cmap_init(&teid2pgw[i]);
        ovs_mutex_init(&teid2pgw_mutex[i]);
        cmap_init(&ueip2pgw[i]);
        ovs_mutex_init(&ueip2pgw_mutex[i]);
    }
    ovs_mutex_unlock(&mutex);
}

void 
gtp_manager_dump(void)
{
    VLOG_INFO("dumping ovs gtp info ........................");
    VLOG_INFO("    ovs_id : %d ", ovs_id);
    VLOG_INFO("    ovs_total : %d ", ovs_total);
    VLOG_INFO("    ovs_phy_port : %d ", ovs_phy_port);
    VLOG_INFO("    pgw_fastpath : %d ", pgw_fastpath);
    VLOG_INFO("    ...........pgw list.............");
    VLOG_INFO("    pgw current : %d ", current);
    VLOG_INFO("    pgw end : %d ", end);
    int i;
    for (i = 0; i < end; ++i)
    {
        if (pgw_list[i].gtp_pgw_ip != 0){
            VLOG_INFO("    pgw %d : gtp_pgw_ip="IP_FMT" gtp_pgw_port=%d gtp_pgw_eth="ETH_ADDR_FMT" pgw_sgi_port=%d pgw_sgi_eth="ETH_ADDR_FMT" ", i, IP_ARGS(pgw_list[i].gtp_pgw_ip), pgw_list[i].gtp_pgw_port, ETH_ADDR_ARGS(pgw_list[i].gtp_pgw_eth), pgw_list[i].pgw_sgi_port, ETH_ADDR_ARGS(pgw_list[i].pgw_sgi_eth));
        }
    }
    VLOG_INFO("    ...........teid->pgw.............");
    for (i = 0; i < HASHMAP_PART_NUM; i++) {
        struct gtp_teid_to_pgw_node *pgw_node;
        CMAP_FOR_EACH (pgw_node, node, &teid2pgw[i]) {
            VLOG_INFO("    teid %d -> { teid4_sgw_c=%d, teid5_sgw_u=%d, teid2_pgw_c=%d, teid3_pgw_u=%d, ue_ip="IP_FMT" pgw_index=%d }", pgw_node->teid, pgw_node->gtp_tunnel_node->teid4_sgw_c, pgw_node->gtp_tunnel_node->teid5_sgw_u, pgw_node->gtp_tunnel_node->teid2_pgw_c, pgw_node->gtp_tunnel_node->teid3_pgw_u, IP_ARGS(pgw_node->gtp_tunnel_node->ue_ip), pgw_node->gtp_tunnel_node->pgw_index);
        }
    }
    VLOG_INFO("    ...........teid->pgw.............");
    for (i = 0; i < HASHMAP_PART_NUM; i++) {
        struct gtp_ueip_to_pgw_node *pgw_node;
        CMAP_FOR_EACH (pgw_node, node, &ueip2pgw[i]) {
            VLOG_INFO("    ueip "IP_FMT" -> { teid4_sgw_c=%d, teid5_sgw_u=%d, teid2_pgw_c=%d, teid3_pgw_u=%d, ue_ip="IP_FMT" pgw_index=%d }", IP_ARGS(pgw_node->ueip), pgw_node->gtp_tunnel_node->teid4_sgw_c, pgw_node->gtp_tunnel_node->teid5_sgw_u, pgw_node->gtp_tunnel_node->teid2_pgw_c, pgw_node->gtp_tunnel_node->teid3_pgw_u, IP_ARGS(pgw_node->gtp_tunnel_node->ue_ip), pgw_node->gtp_tunnel_node->pgw_index);
        }
    }
}


void
gtp_manager_set_params(uint16_t ovsid, uint16_t total, uint16_t phyport, uint8_t fastpath)
{
    ovs_mutex_lock(&mutex);
    ovs_id = ovsid;
    ovs_total = total;
    ovs_phy_port = phyport;
    pgw_fastpath = fastpath;
    ovs_mutex_unlock(&mutex);
}

void
gtp_manager_add_pgw(ovs_be32 gtp_pgw_ip, uint16_t gtp_pgw_port, struct eth_addr gtp_pgw_eth, uint16_t pgw_sgi_port, struct eth_addr pgw_sgi_eth) 
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
        ovs_assert(end < MAX_PGW_PER_NODE);
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
            pgw_list[i].pgw_sgi_port=0;
            break;
        }
    }
}

int
gtp_manager_get_pgw(void)
{
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
        if (pgw_list[i].gtp_pgw_ip == gtp_pgw_ip)
        {
            foundindex = i;
            break;
        }
    }
    return foundindex;
}

int
gtp_manager_put_teid_pgw(uint32_t teid, struct gtp_tunnel_node * gtp_tunnel_node)
{
    uint32_t cmap_id = teid & 0x0000001F;
    struct cmap * teid2pgwmap = &teid2pgw[cmap_id];
    struct gtp_teid_to_pgw_node * node = xmalloc(sizeof *node);
    node->teid = teid;
    node->gtp_tunnel_node = gtp_tunnel_node;
    node->ref_count = 1;
    ovs_mutex_init(&node->mutex);
    ovs_mutex_lock(&teid2pgw_mutex[cmap_id]);
    int n, max;
    uint32_t mask;
    cmap_infos(teid2pgwmap,&n, &max, &mask);
    VLOG_INFO("before put_teid_pgw cmap_id=%d n=%d max=%d mask=%d",cmap_id, n, max, mask);
    size_t ret = cmap_insert(teid2pgwmap, CONST_CAST(struct cmap_node *, &node->node), hash_int(teid, 0));
    cmap_infos(teid2pgwmap,&n, &max, &mask);
    VLOG_INFO("after put_teid_pgw cmap_id=%d n=%d max=%d mask=%d",cmap_id, n, max, mask);  
    ovs_mutex_unlock(&teid2pgw_mutex[cmap_id]);
    return ret;
}

struct gtp_teid_to_pgw_node * 
gtp_manager_get_teid_pgw(uint32_t teid)
{
    uint32_t cmap_id = teid & 0x0000001F;
    struct cmap * teid2pgwmap = &teid2pgw[cmap_id]; 
    struct gtp_teid_to_pgw_node *pgw_node;

    CMAP_FOR_EACH_WITH_HASH (pgw_node, node, hash_int(teid, 0), teid2pgwmap) {
        if (pgw_node->teid == teid) {
            ovs_mutex_lock(&pgw_node->mutex);
            pgw_node->ref_count++;
            ovs_mutex_unlock(&pgw_node->mutex);
            return pgw_node;
        }
    }
    return NULL;
}

int gtp_manager_del_teid_pgw(uint32_t teid)
{
    uint32_t cmap_id = teid & 0x0000001F;
    struct cmap * teid2pgwmap = &teid2pgw[cmap_id];
    uint32_t hash = hash_int(teid, 0);
    struct gtp_teid_to_pgw_node * pgw_node;
    struct gtp_teid_to_pgw_node * found_node;
    CMAP_FOR_EACH_WITH_HASH (pgw_node, node, hash, teid2pgwmap) {
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
    size_t ret = cmap_remove(teid2pgwmap, &found_node->node, hash);
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
    uint32_t cmap_id = (ueip & 0x1F000000) >> 24;
    struct cmap * ueip2pgwmap = &ueip2pgw[cmap_id];
    struct gtp_ueip_to_pgw_node * node = xmalloc(sizeof *node);
    node->ueip = ueip;
    node->gtp_tunnel_node = gtp_tunnel_node;
    node->ref_count = 1;
    ovs_mutex_init(&node->mutex);
    ovs_mutex_lock(&ueip2pgw_mutex[cmap_id]);
    int n, max;
    uint32_t mask;
    cmap_infos(ueip2pgwmap,&n, &max, &mask);
    VLOG_INFO("before put_ueip_pgw cmap_id=%d n=%d max=%d mask=%d",cmap_id, n, max, mask);
    size_t ret = cmap_insert(ueip2pgwmap, CONST_CAST(struct cmap_node *, &node->node),
                hash_int(ueip, 0));
    cmap_infos(ueip2pgwmap,&n, &max, &mask);
    VLOG_INFO("after put_ueip_pgw cmap_id=%d n=%d max=%d mask=%d", cmap_id, n, max, mask);
    ovs_mutex_unlock(&ueip2pgw_mutex[cmap_id]);
    return ret;
}

struct gtp_ueip_to_pgw_node * 
gtp_manager_get_ueip_pgw(uint32_t ueip)
{
    uint32_t cmap_id = (ueip & 0x1F000000) >> 24;
    struct cmap * ueip2pgwmap = &ueip2pgw[cmap_id]; 
    struct gtp_ueip_to_pgw_node *pgw_node;

    CMAP_FOR_EACH_WITH_HASH (pgw_node, node, hash_int(ueip, 0), ueip2pgwmap) {
        if (pgw_node->ueip == ueip) {
            ovs_mutex_lock(&pgw_node->mutex);
            pgw_node->ref_count++;
            ovs_mutex_unlock(&pgw_node->mutex);
            return pgw_node;
        }
    }
    return NULL;
}

int gtp_manager_del_ueip_pgw(uint32_t ueip)
{
    uint32_t cmap_id = (ueip & 0x1F000000) >> 24;
    struct cmap * ueip2pgwmap = &ueip2pgw[cmap_id];
    uint32_t hash = hash_int(ueip, 0);
    struct gtp_ueip_to_pgw_node * pgw_node;
    struct gtp_ueip_to_pgw_node * found_node;
    CMAP_FOR_EACH_WITH_HASH (pgw_node, node, hash, ueip2pgwmap) {
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
    size_t ret = cmap_remove(ueip2pgwmap, &found_node->node, hash);
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

bool maybe_gtpc_message(const struct flow *flow){
    if (is_ip_any(flow) && flow->nw_proto == IPPROTO_UDP && (flow->tp_dst ==htons(GTPC_PORT) || flow->tp_src == htons(GTPC_PORT)) ) {
        return true;
    }
    return false;
}

bool maybe_gtpu_message(const struct flow *flow){
    if (is_ip_any(flow) && flow->nw_proto == IPPROTO_UDP && (flow->tp_dst ==htons(GTPU_PORT) || flow->tp_src == htons(GTPU_PORT)) ) {
        return true;
    }
    return false;
} 

struct gtpc_msg_header *
parse_gtpc_msg_header(const struct dp_packet * packet)
{
    struct gtpc_msg_header * msg = xmalloc(sizeof *msg);
    int header_length = 0;
    int offset = (char *) dp_packet_get_udp_payload(packet) - (char *) dp_packet_data(packet);
    uint8_t * flag = dp_packet_at(packet, offset, 1);
    msg->version = (*flag & 0xe0) >> 5;
    if(msg->version != 2){
        return NULL;
    }
    msg->has_teid = (*flag & 0x08) == 0 ? false:true;
    offset = offset + 1;
    header_length = header_length + 1;
    uint8_t * message_type = dp_packet_at(packet, offset, 1);
    msg->message_type = *message_type;
    offset = offset + 1;
    header_length = header_length + 1;
    ovs_be16 * message_length = dp_packet_at(packet, offset, 2);
    msg->message_length= ntohs(*message_length);
    offset = offset + 2;
    header_length = header_length + 2;
    if (msg->has_teid)
    {
        ovs_be32  * teid = dp_packet_at(packet, offset, 4);
        msg->teid = ntohl(*teid);
        offset = offset + 4;
        header_length = header_length + 4;
    }
    ovs_be32 * seq_num = dp_packet_at(packet, offset, 4);
    msg->seq_num =ntohl((*seq_num) & 0xFFFFFF00) >> 8;
    offset = offset + 4;
    header_length = header_length + 4;
    msg->body_offset = offset; 
    msg->header_length = header_length;

    VLOG_INFO("parse gtpc message : version=%d    seq_num=%#"PRIx32"    message_type=%d    teid=%#"PRIx32".", msg->version, msg->seq_num, msg->message_type, msg->teid); 
    return msg;
}

struct gtpu_msg_header *
parse_gtpu_message(const struct dp_packet * packet)
{
    struct gtpu_msg_header * msg = xmalloc(sizeof *msg);
    int header_length = 0;
    int offset = (char *) dp_packet_get_udp_payload(packet) - (char *) dp_packet_data(packet);
    uint8_t * flag = dp_packet_at(packet, offset, 1);
    msg->version = (*flag & 0xe0) >> 5;
    if(msg->version != 1){
        free(msg);
        return NULL;
    }
    offset = offset + 1;
    header_length = header_length + 1;
    uint8_t * message_type = dp_packet_at(packet, offset, 1);
    msg->message_type = *message_type;
    offset = offset + 1;
    header_length = header_length + 1;
    ovs_be16 * message_length = dp_packet_at(packet, offset, 2);
    msg->message_length= ntohs(*message_length);
    offset = offset + 2;
    header_length = header_length + 2;
    ovs_be32 * teid = dp_packet_at(packet, offset, 4);
    msg->teid = ntohl(*teid);
    offset = offset + 4;
    header_length = header_length + 4;
    msg->body_offset = offset;
    msg->header_length = header_length;

    VLOG_INFO("parse gtpu message : version %d    teid=%#"PRIx32".", msg->version, msg->teid);
    return msg;
}

void
handle_gtpc_message(struct flow *flow, struct flow_wildcards *wc, struct gtpc_msg_header * gtpcmsg, const struct dp_packet * packet, struct xlate_ctx *ctx)
{
    VLOG_INFO("handle gtpc message......");
    if (flow->in_port.ofp_port == ovs_phy_port){
        //from sgw to pgw
        VLOG_INFO("from sgw to pgw......");
        if (!gtpcmsg->has_teid || gtpcmsg->teid == 0)
        {
            if (gtpcmsg->seq_num%ovs_total == ovs_id)
            {
                VLOG_INFO("this ovs %d is selected.", ovs_id);
                int ramdompgw = gtp_manager_get_pgw();
                if (gtpcmsg->message_type == 32)
                {
                    VLOG_INFO("create session request");
                    //create session request
                    if (pgw_fastpath != 0)
                    {
                        VLOG_INFO("fast path is not supported.");
                        //enable fast path
                        //parse body to get t2, t3
                        //create gtp_tunnel_node, set t2, t3, pgwindex
                        //put t2->pnode, t3->pnode, ref_count=2
                    }
                }

                //set dst to pgw ip and mac
                memset(&wc->masks.nw_dst, 0xff, sizeof wc->masks.nw_dst);
                flow->nw_dst = pgw_list[ramdompgw].gtp_pgw_ip;
                WC_MASK_FIELD(wc, dl_dst);
                flow->dl_dst = pgw_list[ramdompgw].gtp_pgw_eth;
                //output to pgw port
                VLOG_INFO("output to pgw port %d.", pgw_list[ramdompgw].gtp_pgw_port);
                xlate_output_action(ctx, pgw_list[ramdompgw].gtp_pgw_port, 0, false);   
            }
            //else drop
        } else {
            struct gtp_teid_to_pgw_node * selectedpgw = gtp_manager_get_teid_pgw(gtpcmsg->teid);
            if (selectedpgw == NULL)
            {
                //drop
                VLOG_INFO("do not find the pgw, drop");
            } else {
                int selectedpgwindex = selectedpgw->gtp_tunnel_node->pgw_index;
                if (pgw_list[selectedpgwindex].gtp_pgw_ip == 0) {
                    //drop
                    VLOG_INFO("the found pgw is deleted.");
                } else {
                    //set dst to pgw ip and mac
                    memset(&wc->masks.nw_dst, 0xff, sizeof wc->masks.nw_dst);
                    flow->nw_dst = pgw_list[selectedpgwindex].gtp_pgw_ip;
                    WC_MASK_FIELD(wc, dl_dst);
                    flow->dl_dst = pgw_list[selectedpgwindex].gtp_pgw_eth;
                    //output to pgw port
                    VLOG_INFO("output to pgw port %d.", pgw_list[selectedpgwindex].gtp_pgw_port);
                    xlate_output_action(ctx, pgw_list[selectedpgwindex].gtp_pgw_port, 0, false);
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
        VLOG_INFO("from pgw to sgw ......");
        if (!gtpcmsg->has_teid || gtpcmsg->teid == 0) {
            //output to phy port
            VLOG_INFO("output to phy port %d.", ovs_phy_port);
            xlate_output_action(ctx, ovs_phy_port, 0, false);
        } else if (gtpcmsg->message_type == 33){
            //create session response
            VLOG_INFO("create session response ......");
            //int ret = gtp_manager_put_teid_pgw(gtpcmsg->teid, flow->nw_src);
            //get t2 from header
            uint32_t t2 = gtpcmsg->teid;
            //continue to parse body
            int start = gtpcmsg->body_offset;
            //Octets 3 to 4 represent the Message Length field. 
            //This field shall indicate the length of the message in octets excluding the mandatory part of the GTP-C header (the first 4 octets).
            int length = gtpcmsg->message_length + 4;
            int finish = start + (length - gtpcmsg->header_length);
            //get t4, t5, ueip
            uint32_t t4 = 0;
            uint32_t t5 = 0;
            ovs_be32 ueip = 0;
            while(start < finish){
                uint8_t * ie_type = dp_packet_at(packet, start, 1);
                ovs_be16 * ie_length = dp_packet_at(packet, start+1, 2);
                uint16_t ie_length_t = ntohs(*ie_length);
                if (*ie_type == 87)
                {
                    //t4
                    ovs_be32 *t4p = dp_packet_at(packet, start+5, 4);
                    t4 = ntohl(*t4p);
                } else if (*ie_type == 79)
                {
                    //ueip
                    ovs_be32 *ueipp = dp_packet_at(packet, start+5, 4);
                    ueip = *ueipp;
                } else if (*ie_type == 93)
                {
                    //bearer context
                    int bcstart = start+4;
                    int bcstartend = bcstart + ie_length_t;
                    while(bcstart < bcstartend){
                        uint8_t * bc_ie_type = dp_packet_at(packet, bcstart, 1);
                        ovs_be16 * bc_ie_length = dp_packet_at(packet, bcstart+1, 2);
                        uint16_t bc_ie_length_t = ntohs(*bc_ie_length);
                        if (*bc_ie_type == 87)
                        {
                            ovs_be32 *t5p = dp_packet_at(packet, bcstart+5, 4);
                            t5 = ntohl(*t5p);
                            break;
                        }
                        bcstart = bcstart + bc_ie_length_t + 4;
                    }
                }
                if (t4 !=0 && t5 !=0 && ueip != 0)
                {
                    break;
                }
                start = start + ie_length_t + 4;
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

                    VLOG_INFO("parse result from CreateSessionResponse : t2=%#"PRIx32", t4=%#"PRIx32", t5=%#"PRIx32", ueip="IP_FMT".", t2, t4, t5, IP_ARGS(ueip));

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
                        VLOG_INFO(" { teid4_sgw_c=%d, teid5_sgw_u=%d, teid2_pgw_c=%d, ue_ip="IP_FMT" pgw_index=%d }", t4, t5, t2, IP_ARGS(ueip), pgwindex);

                        ////refcount = refcount+4
                        //output to phy port
                        VLOG_INFO("output to phy port %d.", ovs_phy_port);
                        xlate_output_action(ctx, ovs_phy_port, 0, false);
                    }

                } else {
                    VLOG_INFO("fast path is not supported ......");
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
            VLOG_INFO("delete session response ......");
            //int ret = gtp_manager_del_teid_pgw(gtpcmsg->teid);
            //teid in header is t2
            uint32_t t2 = gtpcmsg->teid;
            //both fastpath and not fastpath, the t2 is in map, get gtp_tunnel_node from t2
            struct gtp_teid_to_pgw_node * node = gtp_manager_get_teid_pgw(t2);
            uint32_t t4 = node->gtp_tunnel_node->teid4_sgw_c;
            uint32_t t5 = node->gtp_tunnel_node->teid5_sgw_u;
            uint32_t ueip = node->gtp_tunnel_node->ue_ip;
            //delete t2, t3 when fastpath, t4, t5, ueip in map, delete one, unref one, if zero, free the node
            gtp_manager_del_teid_pgw(t4);
            gtp_manager_del_teid_pgw(t5);
            gtp_manager_del_ueip_pgw(ueip);
            gtp_manager_del_teid_pgw(t2);

            VLOG_INFO("delete session : t2=%#"PRIx32", t4=%#"PRIx32", t5=%#"PRIx32", ueip="IP_FMT".", t2, t4, t5, IP_ARGS(ueip));

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
            VLOG_INFO("output to phy port %d.", ovs_phy_port);
            xlate_output_action(ctx, ovs_phy_port, 0, false);
        } else {
            //output to phy port
            VLOG_INFO("output to phy port %d.", ovs_phy_port);
            xlate_output_action(ctx, ovs_phy_port, 0, false);
        } 
    }
}

void
handle_gtpu_message(struct flow *flow, struct flow_wildcards *wc, struct gtpu_msg_header * gtpumsg, const struct dp_packet * packet, struct xlate_ctx *ctx)
{
    VLOG_INFO("handle gtpu message......");
    if (flow->in_port.ofp_port == ovs_phy_port){
        //from sgw to pgw
        VLOG_INFO("from sgw to pgw.");
        if (gtpumsg->teid == 0){
            //drop
            VLOG_INFO("teid is zero, drop.");
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
                    VLOG_INFO("pgw not found, drop.");
                } else {
                    int selectedpgwindex = selectedpgw->gtp_tunnel_node->pgw_index;
                    if (pgw_list[selectedpgwindex].gtp_pgw_ip == 0) {
                        //drop
                        VLOG_INFO("the found pgw is deleted, drop.");
                    } else {
                        //set dst to pgw ip and mac
                        memset(&wc->masks.nw_dst, 0xff, sizeof wc->masks.nw_dst);
                        flow->nw_dst = pgw_list[selectedpgwindex].gtp_pgw_ip;
                        WC_MASK_FIELD(wc, dl_dst);
                        flow->dl_dst = pgw_list[selectedpgwindex].gtp_pgw_eth;
                        //output to pgw port
                        VLOG_INFO("output to pgw port %d.", pgw_list[selectedpgwindex].gtp_pgw_port);
                        xlate_output_action(ctx, pgw_list[selectedpgwindex].gtp_pgw_port, 0, false);
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
                VLOG_INFO("fast path not supported %"PRId64".", (uint64_t)packet);
                //get body from package
                //send the ip msg in body to phy port
                //how? modify the current? drop the current and create new one?
                //learn from arp
            }
        }
    } else {
        //from pgw to sgw
        VLOG_INFO("from sgw to pgw.");
        if (pgw_fastpath == 0)
        {
            //not fast path
            //output to phy port
            VLOG_INFO("output to phy port %d.", ovs_phy_port);
            xlate_output_action(ctx, ovs_phy_port, 0, false);
        } else {
            //should not be here
            VLOG_INFO("fast path not supported.");
        }
    }
}

void handle_gtp(struct flow *flow, struct flow_wildcards *wc, const struct dp_packet * packet, struct xlate_ctx *ctx)
{
    if (packet == NULL){
        VLOG_INFO("packet is null for revalidating");
        return;
    }

    if (maybe_gtpc_message(flow)){
        VLOG_INFO("handle gtpc......");
        struct gtpc_msg_header * gtpcmsgheader = NULL;
        gtpcmsgheader = parse_gtpc_msg_header(packet);
        if(gtpcmsgheader != NULL) {
            handle_gtpc_message(flow, wc, gtpcmsgheader, packet, ctx);
            free(gtpcmsgheader); 
        }                   
    } else if (maybe_gtpu_message(flow)){
        VLOG_INFO("handle gtpu......");
        struct gtpu_msg_header * gtpumsgheader = NULL;
        gtpumsgheader = parse_gtpu_message(packet);
        if(gtpumsgheader != NULL) {
            handle_gtpu_message(flow, wc, gtpumsgheader, packet, ctx);
            free(gtpumsgheader);                    
        }
    }
}

//src_ip=ueip_pool or dst_ip=ueip_pool, should be handled here
void handle_pgw_sgi(struct flow *flow, struct flow_wildcards *wc, const struct dp_packet * packet, struct xlate_ctx *ctx)
{
    if (packet == NULL){
        VLOG_INFO("packet is null for revalidating");
        return;
    }

    VLOG_INFO("handle sgi......");
    if (flow->in_port.ofp_port == ovs_phy_port){
        //from www to pgw
        VLOG_INFO("from www to pgw.");
        if (pgw_fastpath == 0)
        {
            //not fast path
            //dst=ueip
            //get pgw with ueip, if no drop
            //send packet to pgw sgi port, do not change packet
            struct gtp_ueip_to_pgw_node * selectedpgw = gtp_manager_get_ueip_pgw(flow->nw_dst);
            if (selectedpgw == NULL)
            {
                //drop
                VLOG_INFO("can not find pgw.");
            } else {
                int selectedpgwindex = selectedpgw->gtp_tunnel_node->pgw_index;
                if (pgw_list[selectedpgwindex].gtp_pgw_ip == 0) {
                    //drop
                    VLOG_INFO("the found pgw is deleted.");
                } else {
                    VLOG_INFO("output to pgw sgi port %d.", pgw_list[selectedpgwindex].pgw_sgi_port);
                    WC_MASK_FIELD(wc, dl_dst);
                    flow->dl_dst = pgw_list[selectedpgwindex].pgw_sgi_eth;
                    //output to pgw port
                    xlate_output_action(ctx, pgw_list[selectedpgwindex].pgw_sgi_port, 0, false);
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
            VLOG_INFO("fast path not supported %"PRId64".", (uint64_t)packet);
            //fast path
            //get t3 with ueip
            //create new package with t3 and send to phy port
        }

    } else {
        //from pgw to www
        VLOG_INFO("from pgw to www.");
        if (pgw_fastpath == 0)
        {
            //not fastpath
            //output to phy port
            VLOG_INFO("output to phy port %d.", ovs_phy_port);
            xlate_output_action(ctx, ovs_phy_port, 0, false);
        } else {
            VLOG_INFO("fast path not supported.");
            //fast path
            //should not be here
            //handle gtpu should send the package directly
        }
    }
}
