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
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "log.h"
#include "utils/cont.h"
#include "utils/safer.h"
#include "net/kad.h"

kad_guid kad_generate_id()
{
    /* return (kad_guid){ .dw[0] = random(), .dw[1] = random() }; */
    return (kad_guid){ .dd = random() };
}

struct kad_ctx *kad_init()
{
    struct timespec time;
    if (clock_gettime(CLOCK_REALTIME, &time) < 0)
        log_perror("Failed clock_gettime(): %s", errno);
    srandom(time.tv_sec * time.tv_nsec * getpid());

    struct kad_ctx *ctx = malloc(sizeof(struct kad_ctx));
    if (!ctx) {
        log_perror("Failed malloc: %s.", errno);
        return NULL;
    }
    /* Although the node_id should be assigned by the network, it seems a
       common practice to have peers generate a random id themselves. */
    ctx->self_id = kad_generate_id();
    log_debug("node_id=%"PRIx64, ctx->self_id.dd);

    for (size_t i = 0; i < KAD_GUID_SPACE; i++)
        list_init(&ctx->buckets[i]);

    return ctx;
}

void kad_shutdown(struct kad_ctx * ctx)
{
    for (int i = 0; i < KAD_GUID_SPACE; i++) {
        struct list_item bucket = ctx->buckets[i];
        while (!list_is_empty(&bucket)) {
            struct kad_node *node =
                cont(bucket.prev, struct kad_node, item);
            list_delete(bucket.prev);
            free_safer(node);
        }
    }
    free_safer(ctx);
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
static inline size_t kad_bucket_hash(kad_guid self_id, kad_guid peer_id)
{
    kad_guid dist = { .dd = self_id.dd ^ peer_id.dd };
    size_t bucket_idx = 0;
    while (bucket_idx < KAD_GUID_SPACE && dist.dd >= (1ULL << (bucket_idx + 1)))
        bucket_idx++;
    return bucket_idx;
}

/**
 * Retrieves a node from its guid. Possibly sets @bkt_idx to the bucket index
 * of the given node.
 */
static inline struct kad_node*
kad_node_get(struct kad_ctx *ctx, const kad_guid node_id, size_t *bkt_idx)
{
    *bkt_idx = kad_bucket_hash(ctx->self_id, node_id);
    const struct list_item *kad_bucket = &ctx->buckets[*bkt_idx];
    const struct list_item *it = kad_bucket;
    struct kad_node *found;
    list_for(it, kad_bucket) {
        found = cont(it, struct kad_node, item);
        if (found->id.dd == node_id.dd)
            return found;
    }
    return NULL;
}

/**
 * Return 0 on success, -1 on failure, 1 when node unknown.
 */
int kad_node_update(struct kad_ctx *ctx, const kad_guid node_id)
{
    size_t bkt_idx;
    struct kad_node *node = kad_node_get(ctx, node_id, &bkt_idx);
    if (!node)
        return 1;
    /* TODO: check that ip:port hasn't changed. */

    struct timespec time;
    if (clock_gettime(CLOCK_REALTIME, &time) < 0) {
        log_perror("Failed clock_gettime(): %s", errno);
        return -1;
    }

    node->last_seen = time.tv_sec;
    list_delete(&node->item);
    list_append(&ctx->buckets[bkt_idx], &node->item);

    return 0;
}

/**
 * Assumes unknown node, i.e. kad_node_update() did not succeed.
 */
struct kad_node *kad_node_can_insert(struct kad_ctx *ctx,
                                     const kad_guid node_id)
{
    size_t bkt_idx = kad_bucket_hash(ctx->self_id, node_id);
    struct list_item *bucket = &ctx->buckets[bkt_idx];
    size_t count = kad_bucket_count(bucket);
    if (count == KAD_K_CONST)
        return cont(bucket->next, struct kad_node, item);

    return NULL;
}

/**
 * Forcibly inserts a node into the routing table.
 *
 * Assumes unknown node and corresponding bucket not full,
 * i.e. kad_node_can_insert() succeeded.
 */
bool kad_node_insert(struct kad_ctx *ctx, const kad_guid node_id,
                     const char host[], const char service[])
{
    struct timespec time;
    if (clock_gettime(CLOCK_REALTIME, &time) < 0) {
        log_perror("Failed clock_gettime(): %s", errno);
        return false;
    }

    struct kad_node *node = malloc(sizeof(struct kad_node));
    if (!node) {
        log_perror("Failed malloc: %s.", errno);
        return false;
    }

    size_t bkt_idx = kad_bucket_hash(ctx->self_id, node_id);
    node->id = node_id;
    strcpy(node->host, host);
    strcpy(node->service, service);
    node->last_seen = time.tv_sec;
    list_init(&(node->item));
    list_prepend(&ctx->buckets[bkt_idx], &(node->item));

    return true;
}

bool kad_node_delete(struct kad_ctx *ctx, const kad_guid node_id)
{
    struct kad_node *node = kad_node_get(ctx, node_id, NULL);
    if (!node) {
        log_error("Unknown node (id=%"PRIx64").", node_id.dd);
        return false;
    }

    list_delete(&node->item);
    free_safer(node);
    return true;
}
