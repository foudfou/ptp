/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include "log.h"
#include "net/actions.h"
#include "net/socket.h"
#include "utils/safer.h"
#include "events.h"

void free_event(struct event *e)
{
    for (size_t i = 0; i < e->alloc_len; ++i)
        free_safer(e->alloc[i]);
    free(e->self);
}

static bool event_node_data_cb(struct event_args args)
{
    return node_handle_data(args.node_data.timers, args.node_data.sock, args.node_data.kctx);
}
struct event event_node_data = {"node-data", .cb=event_node_data_cb, .args={{{0}}}, .fatal=false,};

static bool event_peer_conn_cb(struct event_args args)
{
    if (peer_conn_accept_all(args.peer_conn.sock, args.peer_conn.peers,
                             args.peer_conn.nfds, args.peer_conn.conf) < 0) {
        log_error("Could not accept tcp connection.");
        return false;
    }
    return true;
}
struct event event_peer_conn = {"peer-conn", .cb=event_peer_conn_cb, .args={{{0}}}, .fatal=true,};

bool event_kad_response_cb(struct event_args args)
{
    return kad_response(args.kad_response.sock, args.kad_response.buf, args.kad_response.addr);
}

bool event_peer_data_cb(struct event_args args)
{
    struct peer *p = peer_find_by_fd(args.peer_data.peers, args.peer_data.fd);
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
    return kad_bootstrap(args.kad_bootstrap.timers, args.kad_bootstrap.conf,
                         args.kad_bootstrap.kctx, args.kad_bootstrap.sock);
}

// cppcheck-suppress unusedFunction
bool event_kad_ping_cb(struct event_args args)
{
    return kad_ping(args.kad_ping.kctx, args.kad_ping.sock, args.kad_ping.node);
}

bool event_kad_find_node_cb(struct event_args args)
{
    return kad_find_node(args.kad_find_node.kctx, args.kad_find_node.sock,
                         args.kad_find_node.node, args.kad_find_node.target);
}

bool event_kad_lookup_cb(struct event_args args)
{
    return kad_lookup_progress(args.kad_lookup.target, args.kad_lookup.timers,
                               args.kad_lookup.kctx, args.kad_lookup.sock);
}
