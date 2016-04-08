#include <config.h>

#include "gtp-manager.h"
#include "openvswitch/vlog.h"

VLOG_DEFINE_THIS_MODULE(gtp_manager);

static struct ovs_mutex mutex;

static uint16 ovs_id OVS_GUARDED_BY(mutex);
static uint16 ovs_total OVS_GUARDED_BY(mutex);
static uint16 ovs_phy_port OVS_GUARDED_BY(mutex);

static struct gtp_pgw_node pgw_list[MAX_PGW_PER_NODE];
static uint16 current;
static uint16 end;

static struct ovs_mutex teid2pgw_mutex[HASHMAP_PART_NUM];
static struct cmap teid2pgw[HASHMAP_PART_NUM];

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
gtp_manager_set_ovs_id(uint16 ovsid, uint16 total, uint16 phyport)
{
    ovs_mutex_lock(&mutex);
    ovs_id = ovsid;
    ovs_total = total;
    ovs_phy_port = phyport;
    ovs_mutex_unlock(&mutex);
}

void
gtp_manager_add_pgw(ovs_be32 gtp_pgw_ip, uint16 gtp_pgw_port) 
{
    bool found = false;
    int i = 0;
    for (i = 0; i < end; ++i)
    {
        if (pgw_list[i].gtp_pgw_ip == 0)
        {
            pgw_list[i].gtp_pgw_ip = gtp_pgw_ip;
            pgw_list[i].gtp_pgw_port = gtp_pgw_port;
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
            break;
        }
    }
}

struct gtp_pgw_node *
gtp_manager_get_pgw(void)
{
    struct gtp_pgw_node * pnode = NULL;
    int i;
    for (i = current; i < end; ++i)
    {
        if (pgw_list[i].gtp_pgw_ip != 0)
        {
            pnode = pgw_list[i];
            current = i + 1;
            break;
        }
    }
    if (pnode == NULL)
    {
        pnode = pgw_list[0];
        current = 1;
    }

    return pnode;
}

int
gtp_manager_put_teid_pgw(uint32 teid, ovs_be32 gtp_pgw_ip)
{
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
    uint32 cmap_id = teid & 0x0000001F;
    cmap teid2pgwmap = teid2pgw[cmap_id];
    struct gtp_teid_to_pgw_node * node = xmalloc(sizeof *node);
    node->teid = teid;
    node->pgw_index = foundindex;
    ovs_mutex_lock(&teid2pgw_mutex[cmap_id]);
    size_t ret = cmap_insert(&teid2pgwmap, CONST_CAST(struct cmap_node *, &node->node),
                hash_int(teid, 0));
    ovs_mutex_unlock(&teid2pgw_mutex[cmap_id]);
    return ret;
}

struct gtp_pgw_node * 
gtp_manager_get_teid_pgw(uint32 teid)
{
    uint32 cmap_id = teid & 0x0000001F;
    cmap teid2pgwmap = teid2pgw[cmap_id]; 
    struct gtp_teid_to_pgw_node *pgw_node;

    CMAP_FOR_EACH_WITH_HASH (pgw_node, node, hash_int(teid, 0), &teid2pgwmap) {
        if (pgw_node->teid == teid && pgw_list[pgw_node->pgw_index].gtp_pgw_ip != 0) {
            return &(pgw_list[pgw_node->pgw_index]);
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

struct gtpc_message *
parse_gtpc_message(struct flow *flow, struct dp_packet * packet)
{
    struct gtpc_message * msg = xmalloc(sizeof *msg);
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
    return msg;
}

struct gtpu_message *
parse_gtpu_message(struct flow *flow, struct dp_packet * packet)
{
    struct gtpu_message * msg = xmalloc(sizeof *msg);
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
    return msg;
}

void
handle_gtpc_message(struct flow *flow, struct gtpc_message * gtpcmsg)
{
    if (flow->in_port.ofp_port == ovs_phy_port){
        //from sgw to pgw
        if (!gtpcmsg->has_teid || gtpcmsg->teid == 0)
        {
            if (gtpcmsg->seq_num%ovs_total == ovs_id)
            {
                struct gtp_pgw_node * ramdompgw = gtp_manager_get_pgw();
                //set dst to pgw ip
                //output to pgw port   
            }
            //else drop
        } else {
            struct gtp_pgw_node * selectedpgw = gtp_manager_get_teid_pgw(gtpcmsg->teid);
            if (selectedpgw == NULL)
            {
                //drop
            } else {
                //set dst to pgw ip
                //output to pgw port
            }

        }
        
    } else {
        //from pgw to sgw
        if (!gtpcmsg->has_teid || gtpcmsg->teid == 0) {
            //output to phy port
        } else if (gtpcmsg->message_type == 33){
            //create session response
            int ret = gtp_manager_put_teid_pgw(gtpcmsg->teid, flow->nw_src);
            //continue to parse body
            //get gtp-c teid for sgw, and put teid->pgw to map
            //get gtp-u teid for sgw, and put teid->pgw to map
            //get ue ip, and put ueip->teid->pgw and teid->ueip->pgw
            //output to phy port
        } else if (gtpcmsg->message_type == 37) {
            //delete session response
            int ret = gtp_manager_del_teid_pgw(gtpcmsg->teid);
        } else {
            //output to phy port
        } 
    }
}

void
handle_gtpu_message(struct flow *flow, struct gtpu_message * gtpumsg)
{
    if (flow->in_port.ofp_port == ovs_phy_port){
        //from sgw to pgw
        if (gtpumsg->teid == 0){
            //drop
        } else {
            
        }
    } else {
        //from pgw to sgw
    }
}

