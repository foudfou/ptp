/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <poll.h>
#include "events.h"
#include "log.h"
#include "net/actions.h"
#include "net/kad/req_lru.h"
#include "net/kad/rpc.h"
#include "net/socket.h"
#include "signals.h"
#include "timers.h"
#include "utils/bits.h"
#include "utils/cont.h"
#include "utils/safer.h"
#include "utils/time.h"
#include "server.h"

#define POLL_EVENTS POLLIN|POLLPRI

// FIXME move to defs.h
#define TIMER_KAD_REFRESH_MILLIS 300000

static int pollfds_update(struct pollfd fds[], const int nlisten,
                          struct list_item *peers)
{
    struct list_item * it = peers;
    int npeer = nlisten;
    list_for(it, peers) {
        struct peer *p = cont(it, struct peer, item);
        if (!p) {
            log_error("Undefined container in list.");
            return npeer;
        }

        fds[npeer].fd = p->fd;
        /* TODO: we will have to add POLLOUT when all data haven't been written
           in one loop, and probably have 1 inbuf and 1 outbuf. */
        fds[npeer].events = POLL_EVENTS;
        npeer++;
    }
    return npeer;
}

/**
 * Main event loop
 *
 * poll(2) is portable and should be sufficient as we don't expect to handle
 * thousands of peer connections.
 *
 * Initially inspired from
 * https://www.ibm.com/support/knowledgecenter/en/ssw_i5_54/rzab6/poll.htm
 *
 * TODO: we could use a thread pool with pipes for dispatching heavy tasks (?)
 * http://people.clarkson.edu/~jmatthew/cs644.archive/cs644.fa2001/proj/locksmith/code/ExampleTest/threadpool.c
 * http://stackoverflow.com/a/6954584/421846
 * https://github.com/Pithikos/C-Thread-Pool/blob/master/thpool.c
 */
bool server_run(const struct config *conf)
{
    bool ret = true;

    if (!clock_res_is_millis()) {
        log_fatal("Time resolution is greater than millisecond. Aborting.");
        return false;
    }

    int sock_tcp = socket_init(SOCK_STREAM, conf->bind_addr, conf->bind_port);
    if (sock_tcp < 0) {
        log_fatal("Failed to start tcp socket. Aborting.");
        return false;
    }
    int sock_udp = socket_init(SOCK_DGRAM, conf->bind_addr, conf->bind_port);
    if (sock_udp < 0) {
        log_fatal("Failed to start udp socket. Aborting.");
        return false;
    }
    log_info("Server started.");

    event_queue evq = {0};

    long long tick_init = now_millis();
    if (tick_init < 0)
        return false;
    log_debug("tick_init=%lld", tick_init);
    struct list_item timers = LIST_ITEM_INIT(timers);

    struct timer timer_kad_refresh = {
        .name="kad-refresh", .delay=TIMER_KAD_REFRESH_MILLIS,
        .event=&event_kad_refresh, .item=LIST_ITEM_INIT(timer_kad_refresh.item)
    };
    timer_init(&timers, &timer_kad_refresh, tick_init);

    struct kad_ctx kctx = {0};
    kctx.timers = &timers;
    kctx.sock = sock_udp;
    struct req_lru reqs_out = {0};
    kctx.reqs_out = &reqs_out;
    int nodes_len = kad_rpc_init(&kctx, conf->conf_dir);
    if (nodes_len == -1) {
        log_fatal("Failed to initialize routes. Aborting.");
        return false;
    }
    else if (nodes_len == 0) {
        struct event *event_kad_bootstrap = malloc(sizeof(struct event));
        if (!event_kad_bootstrap) {
            log_perror(LOG_ERR, "Failed malloc: %s.", errno);
            return false;
        }
        *event_kad_bootstrap = (struct event){
            "kad-bootstrap", .cb=event_kad_bootstrap_cb,
            .args.kad_bootstrap={.conf=conf, .kctx=&kctx},
            .fatal=false, .self=event_kad_bootstrap
        };

        // Need to schedule event instead of adding to event queue, otherwise
        // applied after poll returns.
        struct timer *timer_kad_bootstrap = malloc(sizeof(struct timer));
        if (!timer_kad_bootstrap) {
            log_perror(LOG_ERR, "Failed malloc: %s.", errno);
            free_safer(event_kad_bootstrap);
            return false;
        }
        *timer_kad_bootstrap = (struct timer){
            .name="kad-bootstrap", .delay=0, .once=true,
            .event=event_kad_bootstrap, .self=timer_kad_bootstrap
        };
        timer_init(&timers, timer_kad_bootstrap, tick_init);
    }
    else {
        log_debug("Loaded %d nodes from config.");
    }

    int nlisten = 2;
    int nfds = nlisten + conf->max_peers;
    struct pollfd fds[nfds];
    memset(fds, 0, sizeof(fds));
    fds[0].fd = sock_udp;
    fds[0].events = POLL_EVENTS;
    fds[1].fd = sock_tcp;
    fds[1].events = POLL_EVENTS;
    struct list_item peers = LIST_ITEM_INIT(peers);

    while (true) {

        if (BITS_CHK(sig_events, EV_SIGINT)) {
            BITS_CLR(sig_events, EV_SIGINT);
            log_info("Caught SIGINT. Shutting down.");
            break;
        }

        int timeout = timers_get_soonest(&timers);
        if (timeout < -1) {
            log_fatal("Timeout calculation failed (%d). Aborting.", timeout);
            ret = false;
            break;
        }
        log_debug("Waiting to poll (timeout=%li)...", timeout);
        if (poll(fds, nfds, timeout) < 0) {  // event_wait
            if (errno == EINTR)
                continue;
            else {
                log_perror(LOG_ERR, "Failed poll: %s", errno);
                ret = false;
                break;
            }
        }

        for (int i = 0; i < nfds; i++) {
            // event_get_next
            if (fds[i].revents == 0)
                continue;

            if (!BITS_CHK(fds[i].revents, POLL_EVENTS)) {
                log_error("Unexpected revents: %#x", fds[i].revents);
                ret = false;
                goto server_end;
            }

            if (fds[i].fd == sock_udp) {
                event_node_data.args.node_data.kctx = &kctx;
                if (!event_queue_put(&evq, &event_node_data)) {
                    log_error("Enqueue event '%s' failed.", event_node_data.name);
                }
                continue;
            }

            if (fds[i].fd == sock_tcp) {
                event_peer_conn.args.peer_conn.sock = sock_tcp;
                event_peer_conn.args.peer_conn.peers = &peers;
                event_peer_conn.args.peer_conn.nfds = nfds;
                event_peer_conn.args.peer_conn.conf = conf;
                if (!event_queue_put(&evq, &event_peer_conn)) {
                    log_error("Enqueue event '%s' failed.", event_peer_conn.name);
                }
                continue;
            }

            {
                log_debug("Data available on fd %d.", fds[i].fd);
                struct event *event_peer_data = malloc(sizeof(struct event));
                if (!event_peer_data) {
                    log_perror(LOG_ERR, "Failed malloc: %s.", errno);
                    ret = false;
                    goto server_end;
                }
                *event_peer_data = (struct event){
                    "peer-data", .cb=event_peer_data_cb, .args={{{0}}}, .fatal=true,
                    .self=event_peer_data
                };
                event_peer_data->args.peer_data.peers = &peers;
                event_peer_data->args.peer_data.kctx = &kctx;
                event_peer_data->args.peer_data.fd = fds[i].fd;
                if (!event_queue_put(&evq, event_peer_data)) {
                    log_error("Enqueue event '%s' failed.", event_peer_data->name);
                }
            }

        } /* End loop poll fds */

        if (!timers_apply(&timers, &evq)) {
            log_error("Failed to apply all timers.");
            ret = false;
            break;
        }

        // event_dispatch
        while (event_queue_status(&evq) != QUEUE_STATE_EMPTY) {
            struct event *ev = event_queue_get(&evq);
            if (!ev) {
                log_error("Failed to get event from queue.");
                continue;
            }
            log_debug("Triggering event '%s'.", ev->name);
            if (!ev->cb(ev->args) && ev->fatal) {
                ret = false;
            };
            if (ev->self) {
                free_event(ev->self);
            }
            if (!ret) {
                goto server_end;
            }
        }

        nfds = pollfds_update(fds, nlisten, &peers);

    } /* End event loop */

  server_end:
    peer_conn_close_all(&peers);

    kad_rpc_terminate(&kctx, conf->conf_dir);

    socket_shutdown(sock_tcp);
    socket_shutdown(sock_udp);
    log_info("Server stopped.");
    return ret;
}
