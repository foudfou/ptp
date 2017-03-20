/**
 * In Kademelia, peers are virtually structured as leaves of a binary tree,
 * which can also be vizualized as a ring. Peers are placed in the tree by
 * their node ID, which is a N bits number. The distance(A, B) = A XOR B, which
 * can be interpreted as finding the most common bit prefix btw. 2 nodes. Ex:
 * 0x0100 ^ 0x0110 = 0x0010, common prefix "00". It thus really represents a
 * distance in the tree.
 */
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "log.h"
#include "utils/bits.h"
#include "net/kad/dht.h"

static void kad_generate_id(kad_guid *uid)
{
    for (int i = 0; i < KAD_GUID_SPACE; i++)
        uid->b[i] = (unsigned char)random();
}

struct kad_dht *dht_init()
{
    struct timespec time;
    if (clock_gettime(CLOCK_REALTIME, &time) < 0)
        log_perror(LOG_ERR, "Failed clock_gettime(): %s", errno);
    srandom(time.tv_sec * time.tv_nsec * getpid());

    struct kad_dht *dht = malloc(sizeof(struct kad_dht));
    if (!dht) {
        log_perror(LOG_ERR, "Failed malloc: %s.", errno);
        return NULL;
    }
    /* Although the node_id should be assigned by the network, it seems a
       common practice to have peers generate a random id themselves. */
    kad_generate_id(&dht->self_id);
    char *id = log_fmt_hex(LOG_DEBUG, dht->self_id.b, KAD_GUID_BYTE_SPACE);
    log_debug("node_id=%s", id);
    free_safer(id);

    for (size_t i = 0; i < KAD_GUID_SPACE; i++)
        list_init(&dht->buckets[i]);

    return dht;
}

void dht_terminate(struct kad_dht * dht)
{
    for (int i = 0; i < KAD_GUID_SPACE; i++) {
        struct list_item *bucket = &dht->buckets[i];
        list_free_all(bucket, struct kad_node, item);
    }
    free_safer(dht);
}

static inline size_t kad_bucket_count(const struct list_item *bucket)
{
    size_t count = 0;
    const struct list_item *it = bucket;
    list_for(it, bucket)
        count++;
    return count;
}

/* [Kademlia] For each 0 ≤ i < 160, every node keeps a list of (IP address, UDP
port, Node ID) triples for nodes of distance between 2^i and 2^i+1 from itself,
sorted by time last seen (least-recently seen node at the head). We call these
lists k-buckets. Ex: In a 4 bits space, for node 0 and k=3,
bucket 0 has nodes of distance 1..2
bucket 1 has nodes of distance 2..4
bucket 2 has nodes of distance 4..8
bucket 3 has nodes of distance 8..16
Each bucket will hold up to k active nodes. */
static inline size_t kad_bucket_hash(const kad_guid *self_id,
                                     const kad_guid *peer_id)
{
    kad_guid dist = {0};
    size_t bucket_idx = KAD_GUID_SPACE;
    for (int i = 0; i < KAD_GUID_BYTE_SPACE; ++i) {
        dist.b[i] = self_id->b[i] ^ peer_id->b[i];

        for (int j = 0; j < 8; ++j) {
            bucket_idx--;
            if (BITS_CHK(dist.b[i], (1U << (7 - j))))
                goto guid_end;
        }
    }
  guid_end:
    return bucket_idx;
}

/**
 * Retrieves a node from its guid. Possibly sets @bkt_idx to the bucket index
 * of the given node.
 */
static inline struct kad_node*
dht_get(struct kad_dht *dht, const kad_guid *node_id, size_t *bkt_idx)
{
    size_t bidx = kad_bucket_hash(&dht->self_id, node_id);
    if (bkt_idx)
        *bkt_idx = bidx;
    const struct list_item *kad_bucket = &dht->buckets[bidx];
    const struct list_item *it = kad_bucket;
    struct kad_node *found;
    list_for(it, kad_bucket) {
        found = cont(it, struct kad_node, item);
        if (kad_guid_eq(&found->info.id, node_id))
            return found;
    }
    return NULL;
}

/**
 * Try to update node's data and move it to the end of the bucket. The bucket
 * is thus kept ordered by last_seen time, least recent first.
 *
 * Return 0 on success, -1 on failure, 1 when node unknown.
 */
int dht_update(struct kad_dht *dht, const struct kad_node_info *info)
{
    size_t bkt_idx;
    struct kad_node *node = dht_get(dht, &info->id, &bkt_idx);
    if (!node)
        return 1;
    /* TODO: check that ip:port hasn't changed. */

    struct timespec time;
    if (clock_gettime(CLOCK_REALTIME, &time) < 0) {
        log_perror(LOG_ERR, "Failed clock_gettime(): %s", errno);
        return -1;
    }

    node->last_seen = time.tv_sec;
    list_delete(&node->item);
    list_append(&dht->buckets[bkt_idx], &node->item);

    return 0;
}

/**
 * Check if a node with @node_id can be inserted into the DHT.
 *
 * Returns true if there is room in the target bucket, otherwise sets @old node
 * info to the least recently seen node in the target bucket and returns false.
 *
 * Assumes unknown node, i.e. dht_update() did not succeed.
 */
bool dht_can_insert(struct kad_dht *dht, const kad_guid *node_id,
                    struct kad_node_info *old)
{
    size_t bkt_idx = kad_bucket_hash(&dht->self_id, node_id);
    struct list_item *bucket = &dht->buckets[bkt_idx];
    size_t count = kad_bucket_count(bucket);

    if (count == KAD_K_CONST) {
        struct kad_node *least_recent = cont(bucket->next, struct kad_node, item);
        kad_node_info_cpy(old, &least_recent->info);
        return false;
    }
    return true;
}

static struct kad_node *dht_node_new(const struct kad_node_info *info)
{
    struct timespec time;
    if (clock_gettime(CLOCK_REALTIME, &time) < 0) {
        log_perror(LOG_ERR, "Failed clock_gettime(): %s", errno);
        return NULL;
    }

    struct kad_node *node = malloc(sizeof(struct kad_node));
    if (!node) {
        log_perror(LOG_ERR, "Failed malloc: %s.", errno);
        return NULL;
    }

    kad_node_info_cpy(&node->info, info);
    node->last_seen = time.tv_sec;
    list_init(&(node->item));

    return node;
}

/**
 * Forcibly inserts a node into the routing table.
 *
 * Assumes unknown node and corresponding bucket not full,
 * i.e. dht_can_insert() succeeded.
 */
bool dht_insert(struct kad_dht *dht, const struct kad_node_info *info)
{
    struct kad_node *node = dht_node_new(info);
    if (!node)
        return false;
    size_t bkt_idx = kad_bucket_hash(&dht->self_id, &node->info.id);
    list_append(&dht->buckets[bkt_idx], &node->item);
    char *id = log_fmt_hex(LOG_DEBUG, node->info.id.b, KAD_GUID_BYTE_SPACE);
    log_debug("DHT insert of [%s]:%s (id=%s) into bucket %zu.",
              info->host, info->service, info->id, bkt_idx);
    free_safer(id);
    return true;
}

bool dht_delete(struct kad_dht *dht, const kad_guid *node_id)
{
    struct kad_node *node = dht_get(dht, node_id, NULL);
    if (!node) {
        char *id = log_fmt_hex(LOG_ERR, node_id->b, KAD_GUID_BYTE_SPACE);
        log_error("Unknown node (id=%s).", id);
        free_safer(id);
        return false;
    }

    list_delete(&node->item);
    free_safer(node);
    return true;
}
