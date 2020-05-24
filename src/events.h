/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#ifndef EVENTS_H
#define EVENTS_H

/**
 * Events for the event-loop.
 *
 * Events are created as sort of closures: a callback and its arguments. To
 * schedule them, we insert them into to the event queue either directly or via
 * timers, may it be with zero delay: an event is embedded into a timer, which
 * in turn is appended to the main timer list. The event loop consumes timers,
 * effectively removing them from the list, destroying them, freeing their
 * associated event and its associated allocations.
 *
 * In the future, we may want to distinguish emitted events from event
 * listeners.
 *
 * Some events are static as they will be unique during a loop iteration. The
 * callbacks' arguments must be set at runtime.
 */
#include <netinet/in.h>
#include "net/kad/routes.h"
#include "utils/queue.h"

#define EVENT_NAME_MAX 32

#define EVENT_QUEUE_BIT_LEN 8
QUEUE_GENERATE(event_queue, struct event, EVENT_QUEUE_BIT_LEN)

struct event_args {
    union {
        // TODO use struct server_ctx to simplify
        struct node_data {
            struct list_item *timers;
            int               sock;
            struct kad_ctx   *kctx;
        } node_data;

        struct kad_response {
            int                      sock;
            struct iobuf            *buf;
            struct sockaddr_storage  addr;
        } kad_response;

        struct peer_conn {
            int                  sock;
            struct list_item    *peers;
            int                  nfds;
            const struct config *conf;
        } peer_conn;

        struct peer_data {
            struct list_item *peers;
            struct kad_ctx   *kctx;
            int               fd;
        } peer_data;

        struct kad_refresh {
            void *none;
        } kad_refresh;

        struct kad_bootstrap {
            struct list_item    *timers;
            const struct config *conf;
            // for subsequent node_ping
            struct kad_ctx      *kctx;
            int                  sock;
        } kad_bootstrap;

        struct kad_ping {
            struct kad_ctx       *kctx;
            int                   sock;
            struct kad_node_info  node;
        } kad_ping;

        struct kad_find_node {
            struct kad_ctx       *kctx;
            int                   sock;
            struct kad_node_info  node;
            kad_guid              target;
        } kad_find_node;

        struct {
            kad_guid              target;
            struct list_item     *timers;
            struct kad_ctx       *kctx;
            int                   sock;
        } kad_lookup;
    };
};

typedef bool (*eventHandlerFunc)(struct event_args args);

struct event {
    char               name[EVENT_NAME_MAX];
    eventHandlerFunc   cb;
    /* Arguments must be captured at runtime. */
    struct event_args  args;
    bool               fatal;
    /* Must be set for allocated events. Used to free allocated event after consumption. */
    struct event      *self;
    /* Used to free allocated associated data. Watch freeing order! */
    size_t             alloc_len;
    void              *alloc[];
};

void free_event(struct event *e);

extern struct event event_node_data;
extern struct event event_peer_conn;
extern struct event event_kad_refresh;
// event to be malloc'd
bool event_kad_response_cb(struct event_args args);
bool event_peer_data_cb(struct event_args args);
bool event_kad_bootstrap_cb(struct event_args args);
bool event_kad_ping_cb(struct event_args args);
bool event_kad_find_node_cb(struct event_args args);
bool event_kad_lookup_cb(struct event_args args);

#endif /* EVENTS_H */
