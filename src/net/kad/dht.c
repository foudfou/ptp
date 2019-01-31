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
#include "utils/bitfield.h"
#include "utils/bits.h"
#include "net/kad/dht.h"

static void kad_generate_id(kad_guid *uid)
{
    unsigned char rand[KAD_GUID_SPACE_IN_BYTES];
    for (int i = 0; i < KAD_GUID_SPACE_IN_BYTES; i++)
        rand[i] = (unsigned char)random();
    kad_guid_set(uid, rand);
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
    /* « Node IDs are currently just random 160-bit identifiers, though they
       could equally well be constructed as in Chord. » */
    kad_generate_id(&dht->self_id);
    char *id = log_fmt_hex(LOG_DEBUG, dht->self_id.bytes, KAD_GUID_SPACE_IN_BYTES);
    log_debug("self_id=%s", id);
    free_safer(id);

    for (size_t i = 0; i < KAD_GUID_SPACE_IN_BITS; i++)
        list_init(&dht->buckets[i]);
    list_init(&dht->replacement);

    return dht;
}

void dht_terminate(struct kad_dht * dht)
{
    for (int i = 0; i < KAD_GUID_SPACE_IN_BITS; i++) {
        struct list_item *bucket = &dht->buckets[i];
        list_free_all(bucket, struct kad_node, item);
    }
    struct list_item *repl = &dht->replacement;
    list_free_all(repl, struct kad_node, item);
    free_safer(dht);
}

/* « [Kademlia] For each 0 ≤ i < 160, every node keeps a list of (IP address,
   UDP port, Node ID) triples for nodes of distance between 2^i and 2^i+1 from
   itself, sorted by time last seen (least-recently seen node at the head). We
   call these lists k-buckets. » Ex: In a 4 bits space, for node 0 and k=3,
   bucket 0 has nodes of distance 0..2  = node  0001
   bucket 1 has nodes of distance 2..4  = nodes 001x
   bucket 2 has nodes of distance 4..8  = nodes 01xx
   bucket 3 has nodes of distance 8..16 = nodes 1xxx
   Each bucket will hold up to k active nodes.
*/
static inline size_t kad_bucket_hash(const kad_guid *self_id,
                                     const kad_guid *peer_id)
{
    kad_guid dist;
    BYTE_ARRAY_INIT(kad_guid, dist);
    size_t bucket_idx = KAD_GUID_SPACE_IN_BITS;
    for (int i = 0; i < KAD_GUID_SPACE_IN_BYTES; ++i) {
        dist.bytes[i] = self_id->bytes[i] ^ peer_id->bytes[i];

        for (int j = 0; j < 8; ++j) {
            bucket_idx--;
            if (BITS_CHK(dist.bytes[i], (1U << (7 - j))) ||
                bucket_idx == 0) // in case guid space in bytes and in bits are
                                 // not consistent, like when testing
                goto guid_end;
        }
    }
  guid_end:
    return bucket_idx;
}

static inline size_t
kad_bucket_get_nodes(const struct list_item *bucket,
                     struct kad_node_info nodes[], size_t start,
                     const kad_guid *caller) {
    size_t nodes_pos = start;
    const struct list_item *it = bucket;
    struct kad_node *node;
    list_for(it, bucket) {
        if (nodes_pos >= KAD_K_CONST)
            break;
        node = cont(it, struct kad_node, item);
        if (caller && kad_guid_eq(&node->info.id, caller)) {
            log_debug("%s: ignoring known caller", __func__);
            continue;
        }
        kad_node_info_copy(&nodes[nodes_pos], &node->info);
        nodes_pos += 1;
    }
    return nodes_pos;
}

/**
 * Fills the given `nodes` array with k nodes closest to the `target` node,
 * ignoring the `caller` node if known.
 *
 * Traverse the routing table in ascending xor distance order relative to the
 * target key. http://stackoverflow.com/a/30655403/421846
 */
size_t dht_find_closest(struct kad_dht *dht, const kad_guid *target,
                        struct kad_node_info nodes[], const kad_guid *caller)
{
    size_t nodes_pos = 0;
    bitfield visited[BITFIELD_RESERVE_BITS(KAD_GUID_SPACE_IN_BITS)] = {0};

    size_t bucket_idx = kad_bucket_hash(&dht->self_id, target);
    const struct list_item *bucket = &dht->buckets[bucket_idx];

    int prefix_idx = KAD_GUID_SPACE_IN_BITS - bucket_idx;
    kad_guid prefix_mask, target_next;
    while (prefix_idx >= 0) {
        nodes_pos = kad_bucket_get_nodes(bucket, nodes, nodes_pos, caller);
        BITFIELD_SET(visited, bucket_idx, 1);
        // FIXME: generalize inclusion of __func__
        log_debug("%s: nodes added from bucket %d, total=%zu", __func__, bucket_idx, nodes_pos);
        // log_debug("__bucket_idx=%zu, prefix=%zu, added=%zu", bucket_idx, prefix_idx, nodes_pos);
        if (nodes_pos >= KAD_K_CONST)
            goto filled;

        prefix_idx -= 1;
        kad_guid_reset(&prefix_mask);
        kad_guid_setbit(&prefix_mask, prefix_idx);

        kad_guid_xor(&target_next, target, &prefix_mask);
        bucket_idx = kad_bucket_hash(&dht->self_id, &target_next);
        bucket = &dht->buckets[bucket_idx];
    }

    for (int i = KAD_GUID_SPACE_IN_BITS - 1; i >= 0; i--) {
        if (!BITFIELD_GET(visited, i)) {
            bucket = &dht->buckets[i];
            nodes_pos = kad_bucket_get_nodes(bucket, nodes, nodes_pos, caller);
            BITFIELD_SET(visited, i, 1);
            log_debug("%s: other nodes added from bucket %d, total=%zu", __func__, i, nodes_pos);
        }

        if (nodes_pos >= KAD_K_CONST)
            goto filled;
    }

  filled:
    return nodes_pos;
}

static inline size_t kad_bucket_count(const struct list_item *bucket)
{
    size_t count = 0;
    const struct list_item *it = bucket;
    list_for(it, bucket)
        count++;
    return count;
}

static inline struct kad_node*
dht_get_from_list(const struct list_item *list, const kad_guid *node_id)
{
    const struct list_item *it = list;
    struct kad_node *found;
    list_for(it, list) {
        found = cont(it, struct kad_node, item);
        if (kad_guid_eq(&found->info.id, node_id))
            return found;
    }
    return NULL;
}

/**
 * Try to update node's data and move it to the end of the bucket, or the the
 * beginning of the replacement cache.
 *
 * Buckets are thus kept ordered by ascending last_seen time (least recent
 * first). The replacement list is kept ordered by descending last_seen time
 * (most recent first).
 *
 * Return 0 on success, -1 on failure, 1 when node unknown.
 */
int dht_update(struct kad_dht *dht, const struct kad_node_info *info)
{
    size_t bkt_idx = kad_bucket_hash(&dht->self_id, &info->id);
    struct kad_node *node = dht_get_from_list(&dht->buckets[bkt_idx], &info->id);
    if (!node)
        node = dht_get_from_list(&dht->replacement, &info->id);
    if (!node)
        return 1;
    /* TODO: check that ip:port hasn't changed. */

    struct timespec time;
    if (clock_gettime(CLOCK_REALTIME, &time) < 0) {
        log_perror(LOG_ERR, "Failed clock_gettime(): %s", errno);
        return -1;
    }

    node->last_seen = time.tv_sec;
    node->stale = 0;
    list_delete(&node->item);
    list_append(&dht->buckets[bkt_idx], &node->item);

    return 0;
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

    kad_node_info_copy(&node->info, info);
    node->last_seen = time.tv_sec;
    node->stale = 0;
    list_init(&(node->item));

    return node;
}

/**
 * Inserts a node into the routing table.
 *
 * Assumes unknown node, i.e. dht_update() did not succeed.
 */
bool dht_insert(struct kad_dht *dht, const struct kad_node_info *info)
{
    if (kad_guid_eq(&dht->self_id, &info->id)) {
        log_error("Ignoring DHT insert of node with same id as me.");
        return false;
    }

    struct kad_node *node = dht_node_new(info);
    if (!node)
        return false;

    size_t bkt_idx = kad_bucket_hash(&dht->self_id, &node->info.id);
    struct list_item *bucket = &dht->buckets[bkt_idx];
    size_t count = kad_bucket_count(bucket);
    if (count < KAD_K_CONST) {
        list_append(&dht->buckets[bkt_idx], &node->item);
        log_debug("DHT insert into bucket %zu.", bkt_idx);
    }
    else {
        list_prepend(&dht->replacement, &node->item);
        log_debug("DHT insert into replacement cache.");
    }

    return true;
}

bool dht_delete(struct kad_dht *dht, const kad_guid *node_id)
{
    size_t bkt_idx = kad_bucket_hash(&dht->self_id, node_id);
    struct kad_node *node = dht_get_from_list(&dht->buckets[bkt_idx], node_id);
    if (!node) {
        char *id = log_fmt_hex(LOG_ERR, node_id->bytes, KAD_GUID_SPACE_IN_BYTES);
        log_error("Unknown node (id=%s).", id);
        free_safer(id);
        return false;
    }

    list_delete(&node->item);
    free_safer(node);
    return true;
}
