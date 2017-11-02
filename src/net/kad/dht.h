/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
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
#include "kad_defs.h"
#include "utils/cont.h"
#include "utils/byte_array.h"
#include "utils/list.h"
#include "utils/safer.h"

#define list_free_all(itemp, type, field)                               \
    while (!list_is_empty(itemp)) {                                     \
        type *node =                                                    \
            cont(itemp->prev, type, field);                             \
        list_delete(itemp->prev);                                       \
        free_safer(node);                                               \
    }

/* Byte arrays are not affected by endian issues.
   http://stackoverflow.com/a/4523537/421846 */
BYTE_ARRAY_GENERATE(kad_guid, KAD_GUID_SPACE_IN_BYTES)

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
    int                  stale;
};

struct kad_dht {
    kad_guid         self_id;
    /* The routing table is implemented as hash table: an array of lists
       (buckets) of at most KAD_K_CONST node entries. Instead of using a
       generic hash table implementation, we build a specialized one for
       specific operations on each list. Lists are sorted by construction:
       either we append new nodes at the end, or we update nodes and move them
       to the end. */
    struct list_item buckets[KAD_GUID_SPACE_IN_BITS]; // kad_node list
    /* « To reduce traffic, Kademlia delays probing contacts until it has
       useful messages to send them. When a Kademlia node receives an RPC from
       an unknown contact and the k-bucket for that contact is already full
       with k entries, the node places the new contact in a replacement cache
       of nodes eligible to replace stale k-bucket entries. The next time the
       node queries contacts in the k-bucket, any unresponsive ones can be
       evicted and replaced with entries in the replacement cache. The
       replacement cache is kept sorted by time last seen, with the most
       recently seen entry having the highest priority as a replacement
       candidate. » */
    struct list_item replacement; // kad_node list
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
bool dht_insert(struct kad_dht *dht, const struct kad_node_info *info);
bool dht_delete(struct kad_dht *dht, const kad_guid *node_id);

#endif /* DHT_H */
