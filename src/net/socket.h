/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#ifndef SOCKET_H
#define SOCKET_H

#include <netinet/in.h>
#include <stdbool.h>

#define INET_PORTSTRLEN 6 /* Including terminating null */

bool sock_close(int fd);
int socket_init(const int socktype, const char bind_addr[], const char bind_port[]);
bool socket_shutdown(int sock);

bool sockaddr_storage_fmt(char str[], const struct sockaddr_storage *ss);
bool sockaddr_storage_eq(const struct sockaddr_storage *sa, const struct sockaddr_storage *sb);
bool sockaddr_storage_eq_addr(const struct sockaddr_storage *sa, const struct sockaddr_storage *sb);

#endif /* SOCKET_H */
