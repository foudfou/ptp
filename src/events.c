/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include "log.h"
#include "net/actions.h"
#include "net/socket.h"
#include "events.h"

static bool event_node_data_cb(struct event_args args)
{
    return node_handle_data(args.node_data.sock, args.node_data.kctx);
}
struct event event_node_data = {"node-data", .cb=event_node_data_cb, .args={{{0}}}, .fatal=false,};

static bool event_peer_conn_cb(struct event_args args)
{
    if (peer_conn_accept_all(args.peer_conn.sock, args.peer_conn.peer_list,
                             args.peer_conn.nfds, args.peer_conn.conf) < 0) {
        log_error("Could not accept tcp connection.");
        return false;
    }
    return true;
}
struct event event_peer_conn = {"peer-conn", .cb=event_peer_conn_cb, .args={{{0}}}, .fatal=true,};

bool event_peer_data_cb(struct event_args args)
{
    struct peer *p = peer_find_by_fd(args.peer_data.peer_list, args.peer_data.fd);
    if (!p) {
        log_fatal("Unregistered peer fd=%d.", args.peer_data.fd);
        return false;
    }
    if (peer_conn_handle_data(p, args.peer_data.kctx) == CONN_CLOSED &&
        !peer_conn_close(p)) {
        log_fatal("Could not close connection of peer fd=%d.", args.peer_data.fd);
        return false;
    }
    return true;
}

static bool event_kad_refresh_cb(struct event_args args)
{
    (void)args;
    return kad_refresh(NULL);
}
struct event event_kad_refresh = {"kad-refresh", .cb=event_kad_refresh_cb, .args={{{0}}}, .fatal=false,};

bool event_kad_bootstrap_cb(struct event_args args)
{
    return kad_bootstrap(args.kad_bootstrap.timer_list, args.kad_bootstrap.conf,
                         args.kad_bootstrap.kctx, args.kad_bootstrap.sock);
}

bool event_node_ping_cb(struct event_args args)
{
    return node_ping(args.node_ping.kctx, args.node_ping.sock, args.node_ping.node);
}

bool event_kad_join_cb(struct event_args args)
{
    return kad_join(args.kad_join.kctx, args.kad_join.sock, args.kad_join.nodes, args.kad_join.nodes_len);
}
