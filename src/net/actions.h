/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#ifndef ACTIONS_H
#define ACTIONS_H

/**
 * Server actions. Usually embedded in events.
 */
#include <netinet/in.h>
#include "events.h"
#include "net/kad/rpc.h"
#include "net/msg.h"
#include "options.h"
#include "utils/list.h"

enum conn_ret {CONN_OK, CONN_CLOSED};

struct addr_list {
    struct sockaddr_storage addr;
    struct list_item item;
};

/**
 * A "peer" is a client/server listening on a TCP port that implements some
 * specific protocol (msg). A "node" is a client/server listening on a UDP port
 * implementing the distributed hash table protocol (kad).
 */
struct peer {
    struct list_item        item;
    int                     fd;
    struct sockaddr_storage addr;
    // used for logging = addr:port in hex
    char                    addr_str[32+1+4+1];
    struct proto_msg_parser parser;
};

bool node_handle_data(struct list_item *timers, int sock, struct kad_ctx *kctx);
bool kad_response(int sock, struct iobuf *rsp, struct sockaddr_storage addr);

struct peer* peer_find_by_fd(struct list_item *peers, const int fd);
int peer_conn_accept_all(const int listenfd, struct list_item *peers,
                         const int nfds, const struct config *conf);
int peer_conn_handle_data(struct peer *peer, struct kad_ctx *kctx);
bool peer_conn_close(struct peer *peer);
int peer_conn_close_all(struct list_item *peers);

bool kad_bootstrap(struct list_item *timers, const struct config *conf, struct kad_ctx *kctx, const int sock);
bool kad_ping(struct kad_ctx *kctx, const int sock, const struct kad_node_info node);
bool kad_find_node(struct kad_ctx *kctx, const int sock, const struct kad_node_info node, const kad_guid target);
bool kad_refresh(void *data);

#endif /* ACTIONS_H */
