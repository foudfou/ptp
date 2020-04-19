/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include <errno.h>
#include <time.h>
#include "net/socket.h"
#include "kad/test_util.h"

void kad_node_info_set(struct kad_node_info *dst,
                       const struct kad_node_info_data *src)
{
    struct sockaddr_storage ss = {0};
    if (src->addr4.sin_family == AF_INET) {
        struct sockaddr_in *sa = (struct sockaddr_in*)&ss;
        sa->sin_family = src->addr4.sin_family;
        sa->sin_addr.s_addr = htonl(src->addr4.sin_addr.s_addr);
        sa->sin_port = htons(src->addr4.sin_port);
        *dst = (struct kad_node_info){.id = src->id, .addr = ss, .addr_str = {0}};
    }
    else if (src->addr4.sin_family == AF_INET6) {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6*)&ss;
        sa6->sin6_family = src->addr6.sin6_family;
        memcpy(sa6->sin6_addr.s6_addr, src->addr6.sin6_addr.s6_addr, sizeof(struct in6_addr));
        sa6->sin6_port = htons(src->addr6.sin6_port);
        *dst = (struct kad_node_info){.id = src->id, .addr = ss, .addr_str = {0}};
    }
    else {
        // TODO
    }
}

bool kad_node_info_equals(const struct kad_node_info *got,
                          const struct kad_node_info_data *expected)
{
    struct kad_node_info info = {0};
    kad_node_info_set(&info, expected);

    return
        kad_guid_eq(&got->id, &info.id) &&
        sockaddr_storage_eq(&got->addr, &info.addr);
}

/* https://stackoverflow.com/a/1157217/421846 */
int msleep(long ms)
{
    struct timespec ts;
    int res;

    if (ms < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}
