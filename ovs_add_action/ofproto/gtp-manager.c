#include <config.h>

#include "gtp-manager.h"
#include "openvswitch/vlog.h"

VLOG_DEFINE_THIS_MODULE(gtp_manager);

static struct ovs_mutex mutex;

static uint16 ovs_id OVS_GUARDED_BY(mutex);
static uint16 ovs_total OVS_GUARDED_BY(mutex);

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
gtp_manager_set_ovs_id(uint16 ovsid, uint16 total)
{
    ovs_mutex_lock(&mutex);
    ovs_id = ovsid;
    ovs_total = total;
    ovs_mutex_unlock(&mutex);
}

void
gtp_manager_add_pgw(ovs_be32 gtp_pgw_ip) 
{
    bool found = false;
    int i = 0;
    for (i = 0; i < end; ++i)
    {
        if (pgw_list[i].gtp_pgw_ip == 0)
        {
            pgw_list[i].gtp_pgw_ip = gtp_pgw_ip;
            found = true;
            break;
        }
    }
    if (!found)
    {
        ASSERT(end < MAX_PGW_PER_NODE);
        pgw_list[end++].gtp_pgw_ip = gtp_pgw_ip;
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
            break;
        }
    }
}

ovs_be32
gtp_manager_get_pgw(void)
{
    ovs_be32 ipaddr = 0;
    int i;
    for (i = current; i < end; ++i)
    {
        if (pgw_list[i].gtp_pgw_ip != 0)
        {
            ipaddr = pgw_list[i].gtp_pgw_ip;
            current = i + 1;
            break;
        }
    }
    if (ipaddr == 0)
    {
        ipaddr = pgw_list[0].gtp_pgw_ip;
        current = 1;
    }

    return ipaddr;
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

ovs_be32 
gtp_manager_get_teid_pgw(uint32 teid)
{
    uint32 cmap_id = teid & 0x0000001F;
    cmap teid2pgwmap = teid2pgw[cmap_id]; 
    struct gtp_teid_to_pgw_node *pgw_node;

    CMAP_FOR_EACH_WITH_HASH (pgw_node, node, hash_int(teid, 0), &teid2pgwmap) {
        if (pgw_node->teid == teid && pgw_list[pgw_node->pgw_index].gtp_pgw_ip != 0) {
            return pgw_list[pgw_node->pgw_index].gtp_pgw_ip;
        }
    }
    return 0;
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
