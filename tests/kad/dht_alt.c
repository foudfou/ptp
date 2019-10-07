/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "net/socket.h"
#include "net/kad/dht.c"      // testing static functions

/* _alt tests get a tiny guid space (4 bits) for easier reasoning.

   Consider the following nodes and their dht:

   node 1010:
   bucket 0, node  1011
   bucket 1  nodes 100x (ex: 1001)
   bucket 2  nodes 11xx
   bucket 3  nodes 0xxx

   node 1001:
   bucket 0, node  1000
   bucket 1  nodes 101x
   bucket 2  nodes 11xx
   bucket 3  nodes 0xxx

   node 1001:
   bucket 0  node  1000
   bucket 1  nodes 101x
   bucket 2  nodes 11xx
   bucket 3  nodes 0xxx

   node 1100:
   bucket 0  node  1101
   bucket 1  nodes 111x
   bucket 2  nodes 10xx
   bucket 3  nodes 0xxx

   node 0011:
   bucket 0  node  0010
   bucket 1  nodes 000x
   bucket 2  nodes 01xx
   bucket 3  nodes 1xxx

   Now here are some examples of lookups:

   0. suppose node 1010, find_closest for 1001,
   while not (k nodes added, or all buckets visited) do
   1. find the bucket for target (1001 -> 1) [add bucket nodes], note prefix (= space_len - bucket)
   2. next bucket is: target XOR shifted prefix:
   (1001 XOR 0010 = 1011 => bucket 0)
   (1001 XOR 0100 = 1101 => bucket 2)
   (1001 XOR 1000 = 0001 => bucket 3)
   all buckets visited

   0. suppose node 1010, find_closest for 1100
   while not (k nodes added, or all buckets visited):
   1. find the bucket for target (1100 -> 2), this will be the initial prefix_len (2)
   while prefix_len > 0:
     next bucket is: target XOR shifted prefix:
     (1100 XOR 0100 => 1000 => bucket 1)
     (1100 XOR 1000 => 0100 => bucket 3)
     (prefix_len/bucket_max_reached)
   continue with unvisited => initial prefix downwards,
   while prefix_len < space_len, just traverse sequentially:
     (bucket 0)

   0. suppose node 1010, find_closest for 0011
   while not (k nodes added, or all buckets visited):
   1. find the bucket for target (0011 -> 3), this will be the initial prefix_len (1)
   while prefix_len > 0:
     next bucket is: target XOR shifted prefix:
     (0011 XOR 1000 => 1000 => bucket 1)
     (prefix_len/bucket_max_reached)
   continue with unvisited => initial prefix downwards,
   while prefix_len < space_len, just traverse sequentially:
     (bucket 2)
     (bucket 0)
*/

int main ()
{
    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_CRIT)));
    struct kad_dht *dht = dht_create();

    dht->self_id.bytes[0] = 0xa0; // 0b1010
    char *id = log_fmt_hex(LOG_DEBUG, dht->self_id.bytes, KAD_GUID_SPACE_IN_BYTES);
    log_debug("self_id reset (id=%s)", id);
    free_safer(id);

    // use kad_guid_set() in practice rather than literal initializtion
    struct peer_test {
        struct kad_node_info info;
        int                  bucket;
    } peers[] = {
        {{{{0x10}, true}, {0}, {0}}, 3},
        {{{{0xb0}, true}, {0}, {0}}, 0},
        {{{{0x80}, true}, {0}, {0}}, 1},
        {{{{0xd0}, true}, {0}, {0}}, 2},
        {{{{0x70}, true}, {0}, {0}}, 3},
    };

    struct sockaddr_storage ss = {0};
    struct sockaddr_in *sa = (struct sockaddr_in*)&ss;
    sa->sin_family=AF_INET; sa->sin_port=htons(0x0016);
    sa->sin_addr.s_addr=htonl(0x01010101); peers[0].info.addr = ss;
    sa->sin_addr.s_addr=htonl(0x01010200); peers[1].info.addr = ss;
    sa->sin_addr.s_addr=htonl(0x01010201); peers[2].info.addr = ss;
    sa->sin_addr.s_addr=htonl(0x01010202); peers[3].info.addr = ss;
    sa->sin_addr.s_addr=htonl(0x01010203); peers[4].info.addr = ss;

    struct peer_test *peer = peers;
    struct peer_test *peer_end = peers + sizeof(peers)/sizeof(peers[0]);
    while (peer < peer_end) {
        assert(sockaddr_storage_fmt(peer->info.addr_str, &peer->info.addr));

        assert(dht_insert(dht, &peer->info));

        struct kad_node_info bucket[KAD_K_CONST];
        int bucket_len = kad_bucket_get_nodes(&dht->buckets[peer->bucket], bucket, 0, NULL);
        /* printf("%s == %s\n", peer->info.addr_str, bucket[bucket_len-1].addr_str); */
        assert(sockaddr_storage_cmp4(&peer->info.addr, &bucket[bucket_len-1].addr));

        peer++;
    }

    kad_guid target;
    kad_guid_set(&target, (unsigned char[]){0x90}); // 0b1001
    struct kad_node_info nodes[KAD_K_CONST];
    size_t added = dht_find_closest(dht, &target, nodes, NULL);
    assert(added == 5);

    int peer_order[] = {2, 1, 3, 0, 4};
    for (size_t i = 0; i < added; ++i) {
        assert(sockaddr_storage_cmp4(&nodes[i].addr, &peers[peer_order[i]].info.addr));
    }

    kad_guid_set(&target, (unsigned char[]){0xc0}); // 0b1100
    added = dht_find_closest(dht, &target, nodes, NULL);
    assert(added == 5);
    memcpy(peer_order, (int[]){3, 2, 0, 4, 1}, sizeof(peer_order));
    for (size_t i = 0; i < added; ++i) {
        assert(sockaddr_storage_cmp4(&nodes[i].addr, &peers[peer_order[i]].info.addr));
    }

    kad_guid_set(&target, (unsigned char[]){0x03}); // 0b0011
    added = dht_find_closest(dht, &target, nodes, NULL);
    assert(added == 5);
    memcpy(peer_order, (int[]){0, 4, 2, 3, 1}, sizeof(peer_order));
    for (size_t i = 0; i < added; ++i) {
        assert(sockaddr_storage_cmp4(&nodes[i].addr, &peers[peer_order[i]].info.addr));
    }

    dht_destroy(dht);
    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
