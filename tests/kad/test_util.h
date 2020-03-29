/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#ifndef _KAD_UTILS_H
#define _KAD_UTILS_H

#include "net/kad/dht.h"

/**
 * Helper to initialize sockaddr_storage. Handy to initialize kad_node_info.
 */
struct kad_node_info_data {
    kad_guid id;
    union {
        struct sockaddr_in addr4;
        struct sockaddr_in6 addr6;
    };
};

/**
 * CAUTION: Only use with kad_node_info_*() ! Consumer MUST htons() s_addr and
 * sin_port. This can't be done here because htons(0x1020) is not constant.
 */
#define KAD_TEST_NODES_DECL struct kad_node_info_data kad_test_nodes[] = {      \
    { .id = {.bytes = "abcdefghij0123456789", .is_set = true},          \
      .addr4 = {.sin_family=AF_INET, .sin_addr.s_addr=0xc0a8a80f, .sin_port=0x2f58} }, \
    { .id = {.bytes = "mnopqrstuvwxyz123456", .is_set = true},          \
      .addr4 = {.sin_family=AF_INET, .sin_addr.s_addr=0xc0a8a819, .sin_port=0x2f59} }, \
    { .id = {.bytes = "9876543210jihgfedcba", .is_set = true},          \
      .addr6 = {.sin6_family=AF_INET6, .sin6_addr.s6_addr={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, .sin6_port=0x0203} }, \
    { .id = {.bytes = "654321zyxwvutsrqponm", .is_set = true},          \
      .addr6 = {.sin6_family=AF_INET6, .sin6_addr.s6_addr={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0xaa}, .sin6_port=0x0304} }, \
}

void kad_node_info_set(struct kad_node_info *dst, const struct kad_node_info_data *src);
bool kad_node_info_equals(const struct kad_node_info *got, const struct kad_node_info_data *expected);

int msleep(long ms);

#endif /* _KAD_UTILS_H */
