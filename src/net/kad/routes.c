/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
/**
 * In Kademelia, peers are virtually structured as leaves of a binary tree,
 * which can also be vizualized as a ring. Peers are placed in the tree by
 * their node ID, which is a N bits number. The distance(A, B) = A XOR B, which
 * can be interpreted as finding the most common bit prefix btw. 2 nodes. Ex:
 * 0x0100 ^ 0x0110 = 0x0010, common prefix "00". It thus really represents a
 * distance in the tree.
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "log.h"
#include "file.h"
#include "utils/bitfield.h"
#include "utils/bits.h"
#include "utils/safer.h"
#include "utils/time.h"
#include "net/kad/bencode/routes.h"
#include "net/kad/routes.h"

#define ROUTES_STATE_LEN_IN_BYTES 4096
#define NODES_FILE_LEN_IN_BYTES 512


void rand_init()
{
    struct timespec time;
    if (clock_gettime(CLOCK_REALTIME, &time) < 0)
        log_perror(LOG_ERR, "Failed clock_gettime(): %s", errno);
    srandom(time.tv_sec * time.tv_nsec * getpid());
}

static void kad_generate_id(kad_guid *uid)
{
    unsigned char rand[KAD_GUID_SPACE_IN_BYTES];
    for (int i = 0; i < KAD_GUID_SPACE_IN_BYTES; i++)
        rand[i] = (unsigned char)random();
    kad_guid_set(uid, rand);
}

static void routes_init(struct kad_routes *routes)
{
    memset(&routes->self_id, 0, sizeof(kad_guid));
    for (size_t i = 0; i < KAD_GUID_SPACE_IN_BITS; i++) {
        list_init(&routes->buckets[i]);
        list_init(&routes->replacements[i]);
    }
}

struct kad_routes *routes_create()
{
    struct kad_routes *routes = malloc(sizeof(struct kad_routes));
    if (!routes) {
        log_perror(LOG_ERR, "Failed malloc: %s.", errno);
        return NULL;
    }
    routes_init(routes);

    /* « Node IDs are currently just random 160-bit identifiers, though they
       could equally well be constructed as in Chord. » */
    kad_generate_id(&routes->self_id);
    char *id = log_fmt_hex_dyn(LOG_DEBUG, routes->self_id.bytes, KAD_GUID_SPACE_IN_BYTES);
    log_debug("self_id=%s", id);
    free_safer(id);

    return routes;
}

void routes_destroy(struct kad_routes *routes)
{
    for (int i = 0; i < KAD_GUID_SPACE_IN_BITS; i++) {
        struct list_item *bucket = &routes->buckets[i];
        list_free_all(bucket, struct kad_node, item);
        bucket = &routes->replacements[i];
        list_free_all(bucket, struct kad_node, item);
    }
    free_safer(routes);
}

/**
 * Computes the bucket a peer falls into.
 *
 * « For each 0 ≤ i < 160, every node keeps a list of (IP address, UDP port,
 * Node ID) triples for nodes of distance between 2^i and 2^i+1 from itself,
 * sorted by time last seen (least-recently seen node at the head). We call
 * these lists k-buckets. » Ex: In a 4 bits space,for node 0 and k=3,
 *   bucket 0 has nodes of distance 0..2 = node 0001
 *   bucket 1 has nodes of distance 2..4 = nodes 001x
 *   bucket 2 has nodes of distance 4..8 = nodes 01xx
 *   bucket 3 has nodes of distance 8..16 = nodes 1xxx
 * Each bucket will hold up to k active nodes.
 *
 * This is actually the longest common prefix between two nodes. This expresses
 * the bucket-based proximity but is not equivalent to the XOR distance, which
 * is more precise: « Given two 160-bit identifiers, x and y, Kademlia defines
 * the distance between them as their bitwise exclusive or (XOR) interpreted as
 * an integer, d(x, y) = x ⊕ y. »
 *
 * Note in practice we don't need to actually compute the integer value of the
 * kad distance, which wouldn't fit into any C integer type, just compare
 * distances. See node_heap_cmp().
 */
static inline int kad_bucket_hash(const kad_guid *self_id,
                                  const kad_guid *peer_id)
{
    if (!self_id || !peer_id || !self_id->is_set || !peer_id->is_set)
        return -1;

    int diff = KAD_GUID_SPACE_IN_BITS;
    for (size_t i = 0; i < KAD_GUID_SPACE_IN_BYTES; ++i) {
        // avoid additional kad_guid_xor()
        unsigned char xor = self_id->bytes[i] ^ peer_id->bytes[i];
        unsigned lz = clz(xor);
        diff -= lz;
        if (lz != CHAR_BIT ||
            diff == 0) // in case guid space in bytes and in bits are not
                       // consistent, like when testing
            break;
    }

    return diff > 0 ? diff - 1 : 0;
}

static inline size_t
kad_bucket_get_nodes(const struct list_item *bucket,
                     struct kad_node_info nodes[], size_t nodes_pos,
                     size_t max, const kad_guid *caller) {
    const struct list_item *it = bucket;
    struct kad_node *node;
    size_t bucket_pos = 0;
    list_for(it, bucket) {
        if (bucket_pos >= max)
            break;
        node = cont(it, struct kad_node, item);
        if (caller && kad_guid_eq(&node->info.id, caller)) {
            log_debug("%s: ignoring known caller", __func__);
            continue;
        }
        kad_node_info_copy(&nodes[nodes_pos+bucket_pos], &node->info);
        bucket_pos += 1;
    }
    return bucket_pos;
}

/**
 * Fills the given `nodes` array with k nodes closest to the `target` node,
 * ignoring the `caller` node if known.
 *
 * Traverse the routing table in ascending xor distance order relative to the
 * target key. http://stackoverflow.com/a/30655403/421846
 */
size_t routes_find_closest(struct kad_routes *routes, const kad_guid *target,
                        struct kad_node_info nodes[], const kad_guid *caller)
{
    size_t nodes_pos = 0;
    bitfield visited[BITFIELD_RESERVE_BITS(KAD_GUID_SPACE_IN_BITS)] = {0};

    int bucket_idx = kad_bucket_hash(&routes->self_id, target);
    const struct list_item *bucket = &routes->buckets[bucket_idx];

    int prefix_idx = KAD_GUID_SPACE_IN_BITS - bucket_idx;
    kad_guid prefix_mask, target_next;
    target_next.is_set = true;
    while (prefix_idx >= 0) {
        size_t max = KAD_K_CONST - nodes_pos;
        nodes_pos += kad_bucket_get_nodes(bucket, nodes, nodes_pos, max, caller);
        BITFIELD_SET(visited, bucket_idx, 1);
        // TODO generalize inclusion of __func__
        // log_debug("%s: nodes added from bucket %d, total=%zu", __func__, bucket_idx, nodes_pos);
        // log_debug("__bucket_idx=%zu, prefix=%zu, added=%zu", bucket_idx, prefix_idx, nodes_pos);
        if (nodes_pos >= KAD_K_CONST)
            goto filled;

        prefix_idx -= 1;
        kad_guid_reset(&prefix_mask);
        kad_guid_setbit(&prefix_mask, prefix_idx);

        kad_guid_xor(&target_next, target, &prefix_mask);
        bucket_idx = kad_bucket_hash(&routes->self_id, &target_next);
        bucket = &routes->buckets[bucket_idx];
    }

    for (int i = KAD_GUID_SPACE_IN_BITS - 1; i >= 0; i--) {
        if (!BITFIELD_GET(visited, i)) {
            bucket = &routes->buckets[i];
            size_t max = KAD_K_CONST - nodes_pos;
            nodes_pos += kad_bucket_get_nodes(bucket, nodes, nodes_pos, max, caller);
            BITFIELD_SET(visited, i, 1);
            // log_debug("%s: other nodes added from bucket %d, total=%zu", __func__, i, nodes_pos);
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
routes_get_from_list(const struct list_item *list, const kad_guid *node_id)
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

static struct kad_node *
routes_get_with_bucket(struct kad_routes *routes, const kad_guid *node_id,
                    struct list_item **bucket, size_t *bucket_idx)
{
    int bkt_idx = kad_bucket_hash(&routes->self_id, node_id);
    if (bucket_idx)
        *bucket_idx = bkt_idx;

    struct list_item *bkt = &routes->buckets[bkt_idx];
    struct kad_node *node = routes_get_from_list(bkt, node_id);

    if (!node) {
        bkt = &routes->replacements[bkt_idx];
        node = routes_get_from_list(bkt, node_id);
    }

    if (bucket)
        *bucket = bkt;

    return node;
}

/**
 * Try to update node's data and move it to the end of the bucket, or the
 * beginning of the replacement cache.
 *
 * Buckets are thus kept ordered by ascending last_seen time (least recent
 * first). The replacement list is kept ordered by descending last_seen time
 * (most recent first).
 *
 * « When a Kademlia node receives any message (re- quest or reply) from
 * another node, it updates the appropriate k-bucket for the sender’s node ID.
 * [...] If the appropriate k-bucket is full, however, then the recipient pings
 * the k-bucket’s least-recently seen node to decide what to do. If the
 * least-recently seen node fails to respond, it is evicted from the k-bucket *
 * and the new sender inserted at the tail. Otherwise, if the least-recently
 * seen node responds, it is moved to the tail of the list, and the new
 * sender’s contact is discarded. »
 */
bool routes_update(struct kad_routes *routes, const struct kad_node_info *info, time_t time)
{
    struct list_item *bucket = NULL;
    struct kad_node *node = routes_get_with_bucket(routes, &info->id, &bucket, NULL);
    if (!node)
        return false;

    if (!sockaddr_storage_eq_addr(&node->info.addr, &info->addr)) {
        LOG_FMT_HEX_DECL(id, KAD_GUID_SPACE_IN_BYTES);
        log_fmt_hex(id, KAD_GUID_SPACE_IN_BYTES, info->id.bytes);
        log_warning("Node (%s) changed addr: %s -> %s.", id, node->info.addr_str, info->addr_str);

        node->info.addr = info->addr;
        strcpy(node->info.addr_str, info->addr_str);
    }
    node->last_seen = time;
    node->stale = 0;

    list_delete(&node->item);
    list_append(bucket, &node->item);

    return true;
}

static struct kad_node *
routes_node_new(const struct kad_node_info *info, time_t time)
{
    struct kad_node *node = malloc(sizeof(struct kad_node));
    if (!node) {
        log_perror(LOG_ERR, "Failed malloc: %s.", errno);
        return NULL;
    }
    memset(node, 0, sizeof(struct kad_node));

    list_init(&node->item);
    kad_node_info_copy(&node->info, info);
    node->last_seen = time;

    return node;
}

/**
 * Inserts a node into the routing table.
 */
bool routes_insert(struct kad_routes *routes, const struct kad_node_info *info, time_t time)
{
    if (kad_guid_eq(&routes->self_id, &info->id)) {
        log_error("Ignoring routes insert of node with same id as me.");
        return false;
    }

    size_t bkt_idx = 0;
    struct kad_node *node = routes_get_with_bucket(routes, &info->id, NULL, &bkt_idx);
    if (node) {
        log_warning("Routes insert failed: existing node.");
        return false;
    }

    if (!(node = routes_node_new(info, time)))
        return false;

    struct list_item *bucket = &routes->buckets[bkt_idx];
    if (kad_bucket_count(bucket) < KAD_K_CONST) {
        list_append(bucket, &node->item);
        log_debug("Routes insert into bucket %zu.", bkt_idx);
    }
    else {
        list_prepend(&routes->replacements[bkt_idx], &node->item);
        log_debug("Routes insert into replacement cache.");
    }

    return true;
}

bool routes_upsert(struct kad_routes *routes, const struct kad_node_info *node, time_t time)
{
    bool rv = true;

    char *id = log_fmt_hex_dyn(LOG_DEBUG, node->id.bytes, KAD_GUID_SPACE_IN_BYTES);
    if ((rv = routes_update(routes, node, time)))
        log_debug("Routes update of %s (id=%s).", &node->addr_str, id);
    else if ((rv = routes_insert(routes, node, time)))
        log_debug("Routes insert of %s (id=%s).", &node->addr_str, id);
    else
        log_warning("Failed to upsert kad_node (id=%s)", id);
    free_safer(id);

    return rv;
}

bool routes_delete(struct kad_routes *routes, const kad_guid *node_id)
{
    int bkt_idx = kad_bucket_hash(&routes->self_id, node_id);
    struct kad_node *node = routes_get_from_list(&routes->buckets[bkt_idx], node_id);
    if (!node) {
        LOG_FMT_HEX_DECL(id, KAD_GUID_SPACE_IN_BYTES);
        log_fmt_hex(id, KAD_GUID_SPACE_IN_BYTES, node_id->bytes);
        log_error("Unknown node (id=%s).", id);
        return false;
    }

    list_delete(&node->item);
    free_safer(node);
    return true;
}

bool routes_mark_stale(struct kad_routes *routes, const kad_guid *node_id)
{
    struct kad_node *node = routes_get_with_bucket(routes, node_id, NULL, NULL);
    if (!node)
        return false;
    node->stale++;
    return true;
}

int routes_read(struct kad_routes **routes, const char state_path[])
{
    char buf[ROUTES_STATE_LEN_IN_BYTES];
    size_t buf_len = 0;
    if (!file_read(buf, &buf_len, state_path)) {
        log_error("Failed to read routes state file (%s).", state_path);
        goto fail;
    }

    struct kad_routes_encoded encoded;
    if (!benc_decode_routes(&encoded, buf, buf_len)) {
        log_error("Decoding of routes state file (%s) failed.", state_path);
        goto fail;
    }

    *routes = malloc(sizeof(struct kad_routes));
    if (!*routes) {
        log_perror(LOG_ERR, "Failed malloc: %s.", errno);
        goto fail;
    }
    routes_init(*routes);

    (*routes)->self_id = encoded.self_id;
    char *id = log_fmt_hex_dyn(LOG_DEBUG, (*routes)->self_id.bytes, KAD_GUID_SPACE_IN_BYTES);
    log_debug("self_id=%s", id);
    free_safer(id);

    for (size_t i = 0; i < encoded.nodes_len; i++) {
        if (!routes_insert(*routes, &encoded.nodes[i], 0)) {
            log_error("Routes node insert from encoded [%d] failed.", i);
            goto fail;
        }
    }

    return encoded.nodes_len;

  fail:
    *routes = NULL;
    return -1;
}

int kad_read_bootstrap_nodes(struct kad_node_info nodes[], size_t nodes_len,
                             const char state_path[])
{
    char buf[NODES_FILE_LEN_IN_BYTES];
    size_t buf_len = 0;
    if (!file_read(buf, &buf_len, state_path)) {
        log_error("Failed to read bootsrap nodes file '%s'.", state_path);
        return -1;
    }
    log_debug("Reading bootstrap nodes from file '%s'.", state_path);

    // Since bootstrap file contains node id's, a binary file is fine.
    int nnodes = benc_decode_bootstrap_nodes(nodes, nodes_len, buf, buf_len);
    if (nnodes < 0) {
        log_error("Decoding of bootsrap nodes file (%s) failed.", state_path);
        return -1;
    }

    return nnodes;
}

static size_t
routes_encode(const struct kad_routes *routes, struct kad_routes_encoded *encoded)
{
    encoded->self_id = routes->self_id;
    size_t start = encoded->nodes_len;
    for (size_t i = 0; i < KAD_GUID_SPACE_IN_BITS; i++) {
        encoded->nodes_len += kad_bucket_get_nodes(&routes->buckets[i], encoded->nodes, encoded->nodes_len, KAD_K_CONST, NULL);
    }
    return encoded->nodes_len - start;
}

bool routes_write(const struct kad_routes *routes, const char state_path[]) {
    bool res = true;

    struct kad_routes_encoded encoded = {0};
    routes_encode(routes, &encoded);

    struct iobuf buf = {0};
    if (!benc_encode_routes(&buf, &encoded)) {
        log_error("Encoding of routes state file (%s) failed.", state_path);
        res = false; goto cleanup;
    }

    log_debug("Writing routes state file (%s)", state_path);
    if (!file_write(state_path, buf.buf, buf.pos)) {
        log_error("Failed to write routes state file (%s).", state_path);
        res = false; goto cleanup;
    }

  cleanup:
    iobuf_reset(&buf);
    return res;
}
