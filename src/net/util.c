#include <stdio.h>
#include <string.h>
#include "net/util.h"

// https://beej.us/guide/bgnet/html/multi/sockaddr_inman.html
bool sockaddr_storage_fmt(char str[], const struct sockaddr_storage *ss)
{
    char *p = str;
    if (ss->ss_family == AF_INET) {
        const struct sockaddr_in *sa = (struct sockaddr_in *)ss;
        for (int i=0; i<4; i++) {
            snprintf(p, 3, "%02x",((unsigned char*)(&sa->sin_addr))[i]);
            p += 2;
        }
        snprintf(p, 6, ":%02x%02x",
                 ((unsigned char*)(&sa->sin_port))[0],
                 ((unsigned char*)(&sa->sin_port))[1]);
    }
    else if (ss->ss_family == AF_INET6) {
        const struct sockaddr_in6 *sa = (struct sockaddr_in6 *)ss;
        for (int i=0; i<16; i++) {
            snprintf(p, 3, "%02x",((unsigned char*)(&sa->sin6_addr))[i]);
            p += 2;
        }
        snprintf(p, 6, ":%02x%02x",
                 ((unsigned char*)(&sa->sin6_port))[0],
                 ((unsigned char*)(&sa->sin6_port))[1]);
    }
    else {
        return false;
    }
    return true;
}

bool sockaddr_storage_cmp4(struct sockaddr_storage *a, struct sockaddr_storage *b)
{
    return
        ((struct sockaddr_in*)a)->sin_addr.s_addr ==
        ((struct sockaddr_in*)b)->sin_addr.s_addr &&
        ((struct sockaddr_in*)a)->sin_port ==
        ((struct sockaddr_in*)b)->sin_port;
}

bool sockaddr_storage_cmp6(struct sockaddr_storage *a, struct sockaddr_storage *b)
{
    return
        (0 == memcmp(&((struct sockaddr_in6*)a)->sin6_addr.s6_addr,
                     &((struct sockaddr_in6*)b)->sin6_addr.s6_addr, 16) &&
         ((struct sockaddr_in6*)a)->sin6_port ==
         ((struct sockaddr_in6*)b)->sin6_port);
}
