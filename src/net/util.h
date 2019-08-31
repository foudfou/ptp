#ifndef NET_UTIL_H
#define NET_UTIL_H

#include <netinet/in.h>
#include <stdbool.h>

bool sockaddr_storage_fmt(char str[], const struct sockaddr_storage *ss);
bool sockaddr_storage_cmp4(struct sockaddr_storage *a, struct sockaddr_storage *b);
bool sockaddr_storage_cmp6(struct sockaddr_storage *a, struct sockaddr_storage *b);

#endif /* NET_UTIL_H */
