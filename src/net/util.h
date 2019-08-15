#ifndef NET_UTIL_H
#define NET_UTIL_H

#include <netinet/in.h>
#include <stdbool.h>

bool fmt_sockaddr_storage(char str[], const struct sockaddr_storage *ss);

#endif /* NET_UTIL_H */
