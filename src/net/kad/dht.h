#ifndef DHT_H
#define DHT_H

/**
 * The dht is an opaque hash table-like structure. Its internal elements are
 * not exposed. Possible interactions are limited to insert, delete, update.
 */

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "utils/cont.h"
#include "utils/list.h"
#include "utils/safer.h"

#define KAD_GUID_BYTE_SPACE 20
#define KAD_GUID_SPACE      8*KAD_GUID_BYTE_SPACE
#define KAD_K_CONST         8

#define list_free_all(itemp, type, field)                               \
    while (!list_is_empty(itemp)) {                                     \
        type *node =                                                    \
            cont(itemp->prev, type, field);                             \
        list_delete(itemp->prev);                                       \
        free_safer(node);                                               \
    }

/* Byte arrays are not affected by endian issues.
   http://stackoverflow.com/a/4523537/421846 */
typedef struct { unsigned char b[KAD_GUID_BYTE_SPACE]; } kad_guid;

struct kad_node_info {
    kad_guid id;
    char     host[NI_MAXHOST];
    char     service[NI_MAXSERV];
};

/* Nodes (DHT) are not peers (network). */
struct kad_node {
    struct list_item     item;
    struct kad_node_info info;
    time_t               last_seen;
};

struct kad_dht {
    kad_guid         self_id;
    /* The routing table is implemented as hash table: an array of lists
       (buckets) of at most KAD_K_CONST. Instead of using a generic hash table
       implementation, we build a specialized one for specific operations on
       each list. Lists are sorted by construction: either we append new nodes
       at the end, or we update nodes and move them to the end. */
    struct list_item buckets[KAD_GUID_SPACE]; // kad_node list
};

static inline void kad_node_info_cpy(struct kad_node_info *dst,
                                     const struct kad_node_info *src)
{
    dst->id = src->id;
    strcpy(dst->host, src->host);
    strcpy(dst->service, src->service);
}

struct kad_dht *dht_init();
void dht_terminate(struct kad_dht * dht);

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
 * So the proper sequence is to dht_update(), then dht_can_insert(),
 * and then [FIXME: when exactly] dht_insert().
 */
int dht_update(struct kad_dht *dht, const struct kad_node_info *info);
bool dht_can_insert(struct kad_dht *dht, const kad_guid *node_id,
                    struct kad_node_info *old);
bool dht_insert(struct kad_dht *dht, const struct kad_node_info *info);
bool dht_delete(struct kad_dht *dht, const kad_guid *node_id);

#endif /* DHT_H */
