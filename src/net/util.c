#include <stdio.h>
#include "net/util.h"

// https://beej.us/guide/bgnet/html/multi/sockaddr_inman.html
bool fmt_sockaddr_storage(char str[], const struct sockaddr_storage *ss)
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
