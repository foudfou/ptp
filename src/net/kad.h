#ifndef KAD_H
#define KAD_H

#include <netdb.h>
#include <time.h>
#include "utils/list.h"
#include "net/serialization.h"

/* #define KAD_GUID_SPACE 64 */
/* #define KAD_K_CONST    8 */

/* typedef union u64 kad_guid; */

/* TESTING: BEGIN */
#define KAD_GUID_SPACE 4
#define KAD_K_CONST    2

struct uint4 {
    uint8_t hi : 4;
    uint8_t dd : 4;
};

typedef struct uint4 kad_guid;
/* TESTING: END */

/* Nodes (DHT) are not peers (network). */
struct kad_node {
    struct list_item item;
    kad_guid         id;
    char             host[NI_MAXHOST];
    char             service[NI_MAXSERV];
    time_t           last_seen;
};

struct kad_ctx {
    kad_guid         self_id;
    /* The routing table is implemented as hash table: an array of lists
       (buckets) of at most KAD_K_CONST. Instead of using a generic hash table
       implementation, we build a specialized one for specific operations on
       each list. List are sorted by construction: either we append new nodes
       at the end, or we update nodes and move them to the end. */
    struct list_item buckets[KAD_GUID_SPACE];
};

struct kad_ctx *kad_init();
void kad_shutdown(struct kad_ctx *);

/**
 * « When a Kademlia node receives any message (re- quest or reply) from
 * another node, it updates the appropriate k-bucket for the sender’s node ID.
 * [...] If the appropriate k-bucket is full, however, then the recipient pings
 * the k-bucket’s least-recently seen node to decide what to do. If the
 * least-recently seen node fails to respond, it is evicted from the k-bucket *
 * and the new sender inserted at the tail. Otherwise, if the least-recently
 * seen node responds, it is moved to the tail of the list, and the new
 * sender’s contact is discarded. »
 *
 * So the proper sequence is to kad_node_update(), then kad_node_can_insert(),
 * and then [FIXME: when exactly] kad_node_insert().
 */
int kad_node_update(struct kad_ctx *ctx, const kad_guid node_id);
struct kad_node *kad_node_can_insert(struct kad_ctx *ctx,
                                     const kad_guid node_id);
bool kad_node_insert(struct kad_ctx *ctx, const kad_guid node_id,
                     const char host[], const char service[]);
bool kad_node_delete(struct kad_ctx *ctx, const kad_guid node_id);

#endif /* KAD_H */
