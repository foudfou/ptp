#ifndef KAD_H
#define KAD_H

#include <netdb.h>
#include <stdio.h>
#include <time.h>
#include "utils/list.h"

/* #define KAD_GUID_BYTE_SPACE 8 */
/* #define KAD_K_CONST         8 */
#define KAD_GUID_BYTE_SPACE 2
#define KAD_GUID_SPACE      8*KAD_GUID_BYTE_SPACE
#define KAD_K_CONST         2

/* Byte arrays are not affected by endian issues.
   http://stackoverflow.com/a/4523537/421846 */
typedef struct { unsigned char b[KAD_GUID_BYTE_SPACE]; } kad_guid;

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

/**
 * Returns the guid as a string, which THE CONSUMER MUST FREE.
 */
static inline char *kad_guid_hex(const kad_guid *id)
{
    char *str = malloc(2*KAD_GUID_BYTE_SPACE+1);
    for (size_t i = 0; i < KAD_GUID_BYTE_SPACE; i++)
        sprintf(str + 2*i, "%02x", id->b[i]); // no format string vuln
    str[2*KAD_GUID_BYTE_SPACE] = '\0';
    return str;
}

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
int kad_node_update(struct kad_ctx *ctx, const kad_guid *node_id);
struct kad_node *kad_node_can_insert(struct kad_ctx *ctx,
                                     const kad_guid *node_id);
bool kad_node_insert(struct kad_ctx *ctx, const kad_guid *node_id,
                     const char host[], const char service[]);
bool kad_node_delete(struct kad_ctx *ctx, const kad_guid *node_id);

#endif /* KAD_H */
