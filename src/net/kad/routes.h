/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef ROUTES_H
#define ROUTES_H

/**
 * The routing table is an opaque hash table-like structure. Its internal
 * elements are not exposed. Possible interactions are limited to insert,
 * delete, update.
 */

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include "kad_defs.h"
#include "net/kad/id.h"
#include "net/socket.h"
#include "utils/list.h"

#define ADDR_STR_LEN INET6_ADDRSTRLEN+INET_PORTSTRLEN

struct kad_node_info {
    kad_guid                id;
    struct sockaddr_storage addr;
    // Textual representation of addr for logging/debugging
    char                    addr_str[ADDR_STR_LEN];
};

/* Nodes (DHT) are not peers (network). */
struct kad_node {
    struct list_item     item;
    struct kad_node_info info;
    time_t               last_seen;
    /* « When a contact fails to respond to 5 RPCs in a row, it is considered
       stale.  If a k-bucket is not full or its replacement cache is empty,
       Kademlia merely flags stale contacts rather than remove them. This
       ensures, among other things, that if a node’s own network connection
       goes down teporarily, the node won’t completely void all of its
       k-buckets. » */
    int stale;
};

struct kad_routes {
    kad_guid         self_id;
    /* The routing table is implemented as hash table: an array of lists
       (buckets) of at most KAD_K_CONST node entries. Instead of using a
       generic hash table implementation, we build a specialized one for
       specific operations on each list. Lists are sorted by design: depending
       on if we want to evict nodes (buckets) or get the most recent ones
       (replacements). */
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
    struct list_item replacements[KAD_GUID_SPACE_IN_BITS]; // kad_node list
};

/**
 * Intermediary structure for de-/serialization.
 *
 * While libtorrent seems to only include compact host info (address:port) when
 * serializing routes, and supposingly pinging them when deserializing, we
 * prefer to store the node-id's as well. Besides, we store node-id's as raw
 * bytes network-ordered, while libtorrent uses a readable hex string.
 */
struct kad_routes_encoded {
    kad_guid             self_id;
    struct kad_node_info nodes[KAD_GUID_SPACE_IN_BITS*KAD_K_CONST];
    size_t               nodes_len;
};

void rand_init();

struct kad_routes *routes_create();
void routes_destroy(struct kad_routes * routes);

/* routes_get() intentionally kept internal. */
bool routes_update(struct kad_routes *routes, const struct kad_node_info *info, time_t time);
bool routes_insert(struct kad_routes *routes, const struct kad_node_info *info, time_t time);
bool routes_upsert(struct kad_routes *routes, const struct kad_node_info *node, time_t time);
bool routes_delete(struct kad_routes *routes, const kad_guid *node_id);
bool routes_mark_stale(struct kad_routes *routes, const kad_guid *node_id);
size_t routes_find_closest(struct kad_routes *routes, struct kad_node_info nodes[],
                           const kad_guid *target, const kad_guid *caller);

int routes_read_file(struct kad_routes **routes, const char state_path[]);
bool routes_write_file(const struct kad_routes *routes, const char state_path[]);
int routes_read_nodes_file(struct kad_node_info nodes[], size_t nodes_len, const char state_path[]);

#endif /* ROUTES_H */
