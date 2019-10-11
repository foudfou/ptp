/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#ifndef EVENTS_H
#define EVENTS_H

/**
 * Events for the event-loop.
 *
 * Some events are static as they will be unique during a loop iteration. The
 * callbacks' arguments must be set at runtime.
 */
#include "netinet/in.h"
#include "utils/queue.h"

#define EVENT_NAME_MAX 32

struct event_args {
    union {
        // TODO use struct server_ctx to simplify
        struct node_data {
            int             sock;
            struct kad_ctx *kctx;
        } node_data;

        struct peer_conn {
            int                  sock;
            struct list_item    *peer_list;
            int                  nfds;
            const struct config *conf;
        } peer_conn;

        struct peer_data {
            struct list_item *peer_list;
            struct kad_ctx   *kctx;
            int               fd;
        } peer_data;

        struct kad_refresh {
            void *none;
        } kad_refresh;

        struct kad_bootstrap {
            struct list_item    *timer_list;
            const struct config *conf;
        } kad_bootstrap;

        struct node_ping {
            struct sockaddr_storage addr;
        } node_ping;
    };
};

typedef bool (*eventHandlerFunc)(struct event_args args);

struct event {
    char                name[EVENT_NAME_MAX];
    eventHandlerFunc    cb;
    /* Arguments must be captured at runtime. */
    struct event_args   args;
    bool                fatal;
    /* Must be set for allocated events. Used to free allocated event. */
    struct event       *self;
};

struct event event_node_data;
struct event event_peer_conn;
struct event event_kad_refresh;
// event to be malloc'd
bool event_peer_data_cb(struct event_args args);
bool event_kad_bootstrap_cb(struct event_args args);
bool event_node_ping_cb(struct event_args args);

#define EVENT_QUEUE_BIT_LEN 8
QUEUE_GENERATE(event_queue, struct event, EVENT_QUEUE_BIT_LEN)

#endif /* EVENTS_H */
