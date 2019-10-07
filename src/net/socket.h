/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#ifndef SOCKET_H
#define SOCKET_H

#include <netinet/in.h>
#include <stdbool.h>

bool sock_close(int fd);
int socket_init(const int socktype, const char bind_addr[], const char bind_port[]);
bool socket_shutdown(int sock);

bool sockaddr_storage_fmt(char str[], const struct sockaddr_storage *ss);
bool sockaddr_storage_cmp4(const struct sockaddr_storage *a, const struct sockaddr_storage *b);
bool sockaddr_storage_cmp6(const struct sockaddr_storage *a, const struct sockaddr_storage *b);

#endif /* SOCKET_H */
