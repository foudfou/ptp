/**
 * In Kademelia, peers are virtually structured as leaves of a binary tree,
 * which can also be vizualized as a ring. Peers are placed in the tree by
 * their node ID, which is a N bits number. The distance(A, B) = A XOR B, which
 * can be interpreted as finding the most common bit prefix btw. 2 nodes. Ex:
 * 0x0100 ^ 0x0110 = 0x0010, common prefix "00". It thus really represents a
 * distance in the tree.
 */
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include "log.h"
#include "utils/list.h"
#include "proto/kad.h"

/* [Kademlia] For each 0 ≤ i < 160, every node keeps a list of (IP address, UDP
port, Node ID) triples for nodes of distance between 2^i and 2^i+1 from itself,
sorted by time last seen (least-recently seen node at the head). We call these
lists k-buckets. Ex: In a 4 bits space, for node 0 and k=3,
bucket 0 has nodes of distance 1..2
bucket 1 has nodes of distance 2..4
bucket 2 has nodes of distance 4..8
bucket 3 has nodes of distance 8..16
Each bucket will hold up to k active nodes. */
static size_t kad_buckets_hash(kad_guid self_id, kad_guid peer_id)
{
    kad_guid dist = { .dd = self_id.dd ^ peer_id.dd };
    size_t bucket_idx = 0;
    while (bucket_idx < KAD_GUID_SPACE && dist.dd >= (1ULL << (bucket_idx + 1)))
        bucket_idx++;
    return bucket_idx;
}

kad_guid kad_init()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    srandom(((time.tv_sec * 1000) + (time.tv_usec / 1000)) * getpid());

    /* Although the node_id should be assigned by the network, it seems a
       common practice to have peers generate a random id themselves. */
    kad_guid node_id = kad_generate_id();
    log_debug("node_id=%"PRIx64, node_id.dd);

    /* The routing table is implemented as hash table: an array of lists of at
       most KAD_K_CONST (buckets). Instead of using a generic hash table
       implementation, we build a specialized one for specific operations on
       each list. */
    struct list_item kad_buckets[KAD_GUID_SPACE];

    return node_id;
}

void kad_shutdown()
{
    /* TODO: */
}

kad_guid kad_generate_id()
{
    /* return (kad_guid){ .dw[0] = random(), .dw[1] = random() }; */
    return (kad_guid){ .dd = random() };
}
