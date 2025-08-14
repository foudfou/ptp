/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "net/socket.h"
#include "utils/array.h"
#include "net/kad/routes.c"      // testing static functions

/* _alt tests get a tiny guid space (4 bits) for easier reasoning.

   Consider the following nodes and their routes:

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
    struct kad_routes *routes = routes_create();

    routes->self_id.bytes[0] = 0b10100000 /* 0xa0 */;
    char *id = log_fmt_hex_dyn(LOG_DEBUG, routes->self_id.bytes, KAD_GUID_SPACE_IN_BYTES);
    log_debug("self_id reset (id=%s)", id);
    free_safer(id);

    // use kad_guid_set() in practice rather than literal initializtion
    struct peer_test {
        struct kad_node_info info;
        int                  bucket;
    } peers[] = {
        // Only using first 4 bits (KAD_GUID_SPACE_IN_BITS=4)
        //  0b10100000 /* 0xa0 */ self
        {{{{0b00010000 /* 0x10 */}, true}, {0}, {0}}, 0b0011 /* 3 */},
        {{{{0b10110000 /* 0xb0 */}, true}, {0}, {0}}, 0b0000 /* 0 */},
        {{{{0b10000000 /* 0x80 */}, true}, {0}, {0}}, 0b0001 /* 1 */},
        {{{{0b11010000 /* 0xd0 */}, true}, {0}, {0}}, 0b0010 /* 2 */},
        {{{{0b01110000 /* 0x70 */}, true}, {0}, {0}}, 0b0011 /* 3 */},
    };

    assert(kad_bucket_hash(
               &(kad_guid){.bytes = {0}, .is_set = true},
               &(kad_guid){.bytes = {0}, .is_set = true})
           == 0);

    struct sockaddr_storage ss = {0};
    struct sockaddr_in *sa = (struct sockaddr_in*)&ss;
    sa->sin_family=AF_INET; sa->sin_port=htons(0x0016);
    sa->sin_addr.s_addr=htonl(0x01010101); peers[0].info.addr = ss;
    sa->sin_addr.s_addr=htonl(0x01010200); peers[1].info.addr = ss;
    sa->sin_addr.s_addr=htonl(0x01010201); peers[2].info.addr = ss;
    sa->sin_addr.s_addr=htonl(0x01010202); peers[3].info.addr = ss;
    sa->sin_addr.s_addr=htonl(0x01010203); peers[4].info.addr = ss;

    struct peer_test *peer = peers;
    const struct peer_test *peer_end = peers + ARRAY_LEN(peers);
    while (peer < peer_end) {
        assert(sockaddr_storage_fmt(peer->info.addr_str, &peer->info.addr));

        assert(routes_insert(routes, &peer->info, 0));

        struct kad_node_info bucket[KAD_K_CONST];
        int bucket_len = kad_bucket_get_nodes(&routes->buckets[peer->bucket], bucket, 0, KAD_K_CONST, NULL);
        /* printf("%s == %s\n", peer->info.addr_str, bucket[bucket_len-1].addr_str); */
        assert(sockaddr_storage_eq(&peer->info.addr, &bucket[bucket_len-1].addr));

        peer++;
    }

    kad_guid target;
    kad_guid_set(&target, (unsigned char[]){0x90 /* 0b1001 */});
    struct kad_node_info nodes[KAD_K_CONST];
    size_t added = routes_find_closest(routes, nodes, &target, NULL);
    /* printf("added=%zul\n", added); */
    assert(added == 5);

    int peer_order[KAD_K_CONST] = {4, 0, 3, 1, 2, 0};
    for (size_t i = 0; i < added; ++i) {
        assert(sockaddr_storage_eq(&nodes[i].addr, &peers[peer_order[i]].info.addr));
    }

    kad_guid_set(&target, (unsigned char[]){0xc0 /* 0b1100 */});
    added = routes_find_closest(routes, nodes, &target, NULL);
    assert(added == 5);
    memcpy(peer_order, (int[]){0, 4, 1, 2, 3, 0}, sizeof(peer_order));
    for (size_t i = 0; i < added; ++i) {
        assert(sockaddr_storage_eq(&nodes[i].addr, &peers[peer_order[i]].info.addr));
    }

    kad_guid_set(&target, (unsigned char[]){0x03 /* 0b0011 */});
    added = routes_find_closest(routes, nodes, &target, NULL);
    assert(added == 5);
    memcpy(peer_order, (int[]){3, 1, 2, 4, 0, 0}, sizeof(peer_order));
    for (size_t i = 0; i < added; ++i) {
        assert(sockaddr_storage_eq(&nodes[i].addr, &peers[peer_order[i]].info.addr));
    }

    // Known nodes > KAD_K_CONST
    struct kad_node_info peers8[] = {
        //  0b10100000 /* 0xa0 */ self
        {{{0b00010000 /* 0x10 */}, true}, {0}, {0}},
        {{{0b00110000 /* 0x30 */}, true}, {0}, {0}},
        {{{0b01000000 /* 0x40 */}, true}, {0}, {0}},
        {{{0b01110000 /* 0x70 */}, true}, {0}, {0}},
        {{{0b10000000 /* 0x80 */}, true}, {0}, {0}},
        {{{0b10010000 /* 0x90 */}, true}, {0}, {0}},
        {{{0b10110000 /* 0xb0 */}, true}, {0}, {0}},
        {{{0b11010000 /* 0xd0 */}, true}, {0}, {0}},
    };

    for (struct peer_test *p = peers; p != peers + ARRAY_LEN(peers); ++p) {
        assert(routes_delete(routes, &p->info.id));
    }

    for (struct kad_node_info *p = peers8; p != peers8 + ARRAY_LEN(peers8); ++p) {
        assert(routes_insert(routes, p, 0));
    }

    kad_guid_set(&target, (unsigned char[]){0xe0 /* 0b1110 */});
    added = routes_find_closest(routes, nodes, &target, NULL);
    assert(added == 6);

    memcpy(peer_order, (int[]){7, 2, 3, 5, 4, 6}, sizeof(peer_order));
    for (size_t i = 0; i < added; ++i) {
        LOG_FMT_HEX_DECL(id, KAD_GUID_SPACE_IN_BYTES); // cppcheck-suppress shadowVariable
        log_fmt_hex(id, KAD_GUID_SPACE_IN_BYTES, nodes[i].id.bytes);
        log_debug("nodes[%d], id=%s", i, id);
        assert(kad_guid_eq(&nodes[i].id, &peers8[peer_order[i]].id));
    }

    // Ignoring caller
    kad_guid_set(&target, (unsigned char[]){0xe0 /* 0b1110 */});
    added = routes_find_closest(routes, nodes, &target, &peers8[4].id /* 0x80 */);
    assert(added == 6);

    memcpy(peer_order, (int[]){1, 2, 3, 5, 6, 7}, sizeof(peer_order));
    for (size_t i = 0; i < added; ++i) {
        LOG_FMT_HEX_DECL(id, KAD_GUID_SPACE_IN_BYTES); // cppcheck-suppress shadowVariable
        log_fmt_hex(id, KAD_GUID_SPACE_IN_BYTES, nodes[i].id.bytes);
        log_debug("nodes[%d], id=%s", i, id);
        assert(kad_guid_eq(&nodes[i].id, &peers8[peer_order[i]].id));
    }



    routes_destroy(routes);
    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
