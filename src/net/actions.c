/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include <unistd.h>
#include "config.h"
#include "log.h"
#include "net/kad/rpc.h"
#include "net/kad/req_lru.h"
#include "net/socket.h"
#include "timers.h"
#include "utils/array.h"
#include "utils/safer.h"
#include "utils/time.h"
#include "net/actions.h"


#define BOOTSTRAP_FILENAME "nodes.dat"
#define BOOTSTRAP_NODES_LEN 64
// FIXME: low for testing purpose.
#define SERVER_TCP_BUFLEN 10
#define SERVER_UDP_BUFLEN 1400
#define KAD_LOOKUP_TIMEOUT_MILLIS 250


bool node_handle_data(struct kad_ctx *kctx)
{
    bool ret = true;

    char buf[SERVER_UDP_BUFLEN];
    memset(buf, 0, SERVER_UDP_BUFLEN);
    struct sockaddr_storage node_addr;
    socklen_t node_addr_len = sizeof(struct sockaddr_storage);
    ssize_t slen = recvfrom(kctx->sock, buf, SERVER_UDP_BUFLEN, 0,
                            (struct sockaddr *)&node_addr, &node_addr_len);
    if (slen < 0) {
        if (errno != EWOULDBLOCK) {
            log_perror(LOG_ERR, "Failed recv: %s", errno);
            return false;
        }
        return true;
    }

    char addr_str[INET6_ADDRSTRLEN+INET_PORTSTRLEN];
    sockaddr_storage_fmt(addr_str, &node_addr);
    log_debug("Received %d bytes from %s.", slen, addr_str);

    struct iobuf *rsp = malloc(sizeof(struct iobuf));
    if (!rsp) {
        log_perror(LOG_ERR, "Failed malloc: %s.", errno);
        return false;
    }
    memset(rsp, 0, sizeof(struct iobuf));

    bool resp = kad_rpc_handle(kctx, &node_addr, buf, (size_t)slen, rsp);
    if (rsp->pos == 0) {
        log_info("Handling incoming message doesn't need further response.");
        ret = resp; goto cleanup;
    }
    if (rsp->pos > SERVER_UDP_BUFLEN) {
        log_error("Response too large.");
        ret = false; goto cleanup;
    }

    size_t nsub = 1;
    struct event *evt = malloc(sizeof(struct event) + sizeof(void*[nsub]));
    if (!evt) {
        log_perror(LOG_ERR, "Failed malloc: %s.", errno);
        ret = false; goto cleanup;
    }
    *evt = (struct event){
        "kad-response", .cb=event_kad_response_cb,
        .args.kad_response={.sock=kctx->sock, .buf=rsp, .addr=node_addr},
        .fatal=false, .self=evt, .alloc_len=nsub
    };
    evt->alloc[0] = rsp;

    if (!set_timeout(kctx->timers, 0, true, evt)) {
        ret = false; goto cleanup2;
    }

    return ret;

  cleanup2:
    free_safer(evt);
  cleanup:
    iobuf_reset(rsp);
    free_safer(rsp);
    return ret;
}

bool kad_response(int sock, struct iobuf *rsp, struct sockaddr_storage addr)
{
    bool ret = true;

    socklen_t addr_len = sizeof(struct sockaddr_storage);
    ssize_t slen = sendto(sock, rsp->buf, rsp->pos, 0,
                          (struct sockaddr *)&addr, addr_len);
    if (slen < 0) {
        if (errno != EWOULDBLOCK) {
            log_perror(LOG_ERR, "Failed sendto: %s", errno);
            ret = false; goto cleanup;
        }
        goto cleanup;
    }
    log_debug("Sent %d bytes.", slen);

  cleanup:
    iobuf_reset(rsp);
    return ret;
}

static struct peer*
peer_register(struct list_item *peers, int conn, struct sockaddr_storage *addr)
{
    struct peer *peer = calloc(1, sizeof(struct peer));
    if (!peer) {
        log_perror(LOG_ERR, "Failed malloc: %s.", errno);
        return NULL;
    }

    peer->fd = conn;
    peer->addr = *addr;
    sockaddr_storage_fmt(peer->addr_str, &peer->addr);
    proto_msg_parser_init(&peer->parser);
    list_init(&(peer->item));
    list_append(peers, &(peer->item));
    log_debug("Peer %s registered (fd=%d).", peer->addr_str, conn);

    return peer;
}

/**
 * Drain all incoming connections
 *
 * Returns 0 on success, -1 on error, 1 when max_peers reached.
 */
int peer_conn_accept_all(const int listenfd, struct list_item *peers,
                                const int nfds, const struct config *conf)
{
    struct sockaddr_storage peer_addr = {0};
    socklen_t peer_addr_len = sizeof(peer_addr);
    int conn = -1;
    int fail = 0, skipped = 0;
    int npeer = nfds;
    do {
        conn = accept(listenfd, (struct sockaddr *)&peer_addr, &peer_addr_len);
        if (conn < 0) {
            if (errno != EWOULDBLOCK) {
                log_perror(LOG_ERR, "Failed server_conn_accept: %s.", errno);
                return -1;
            }
            break;
        }
        log_debug("Incoming connection...");

        /* Close the connection nicely when max_peers reached. Another approach
           would be to close the listening socket and reopen it when we're
           ready, which would result in ECONNREFUSED on the client side. */
        if ((size_t)npeer > conf->max_peers) {
            log_error("Can't accept new connections: maximum number of peers"
                      " reached (%d/%zd). conn=%d", npeer - 1, conf->max_peers, conn);
            const char err[] = "Too many connections. Please try later...\n";
            send(conn, err, strlen(err), 0);
            sock_close(conn);
            skipped++;
            continue;
        }

        struct peer *p = peer_register(peers, conn, &peer_addr);
        if (!p) {
            log_error("Failed to register peer fd=%d."
                      " Trying to close connection gracefully.", conn);
            if (sock_close(conn))
                skipped++;
            else { // we have more open connections than actually registered...
                log_fatal("Failed to close connection fd=%d. INCONSISTENT STATE");
                fail++;
            }
            continue;
        }
        log_info("Accepted connection from peer %s.", p->addr_str);
        npeer++;

    } while (conn != -1);

    if (fail)
        return -1;
    else if (skipped)
        return 1;
    else
        return 0;
}

struct peer*
peer_find_by_fd(struct list_item *peers, const int fd)
{
    struct peer *p;
    struct list_item * it = peers;
    list_for(it, peers) {
        p = cont(it, struct peer, item);
        if (!p) {
            log_error("Undefined container in list.");
            return NULL;
        }
        if (p->fd == fd)
            break;
    }

    if (it == peers) {
        log_warning("Peer not found fd=%d.", fd);
        return NULL;
    }

    return p;
}

static void peer_unregister(struct peer *peer)
{
    log_debug("Unregistering peer %s.", peer->addr_str);
    proto_msg_parser_terminate(&peer->parser);
    list_delete(&peer->item);
    free_safer(peer);
}

static bool peer_msg_send(const struct peer *peer, enum proto_msg_type typ,
                          const char *msg, union u32 msg_len)
{
    size_t buf_len = msg_len.dd + PROTO_MSG_FIELD_TYPE_LEN + PROTO_MSG_FIELD_LENGTH_LEN;
    char buf[buf_len];
    char *bufp = buf;
    memcpy(bufp, lookup_by_id(proto_msg_type_names, typ), PROTO_MSG_FIELD_LENGTH_LEN);
    bufp += PROTO_MSG_FIELD_TYPE_LEN;
    memcpy(bufp, u32_hton(msg_len).db, PROTO_MSG_FIELD_LENGTH_LEN);
    bufp += PROTO_MSG_FIELD_LENGTH_LEN;
    memcpy(bufp, msg, (size_t)msg_len.dd);

    int resp = send(peer->fd, buf, buf_len, MSG_NOSIGNAL);
    if (resp < 0) {
        if (errno == EPIPE)
            log_info("Peer fd=%u disconnected while sending.");
        else
            log_perror(LOG_ERR, "Failed send: %s.", errno);
        return false;
    }

    return true;
}

int peer_conn_handle_data(struct peer *peer, struct kad_ctx *kctx)
{
    (void)kctx; // FIXME:
    enum conn_ret ret = CONN_OK;

    char buf[SERVER_TCP_BUFLEN];
    memset(buf, 0, SERVER_TCP_BUFLEN);
    ssize_t slen = recv(peer->fd, buf, SERVER_TCP_BUFLEN, 0);
    if (slen < 0) {
        if (errno != EWOULDBLOCK) {
            log_perror(LOG_ERR, "Failed recv: %s", errno);
            ret = CONN_CLOSED;
        }
        goto end;
    }

    if (slen == 0) {
        log_info("Peer %s closed connection.", peer->addr_str);
        ret = CONN_CLOSED;
        goto end;
    }
    log_debug("Received %d bytes.", slen);

    if (peer->parser.stage == PROTO_MSG_STAGE_ERROR) {
        LOG_FMT_HEX_DECL(bufx, slen);
        log_fmt_hex(bufx, slen, (unsigned char*)buf);
        log_error("Parsing error. buf=%s", bufx);
        goto end;
    }

    if (!proto_msg_parse(&peer->parser, buf, slen)) {
        log_debug("Failed parsing of chunk.");
        /* TODO: how do we get out of the error state ? We could send a
           PROTO_MSG_TYPE_ERROR, then watch for a special PROTO_MSG_TYPE_RESET
           msg (RSET64FE*64). But how about we just close the connection. */
        const char err[] = "Could not parse chunk.";
        union u32 err_len = {strlen(err)};
        if (peer_msg_send(peer, PROTO_MSG_TYPE_ERROR, err, err_len)) {
            log_info("Notified peer %s of error state.", peer->addr_str);
        }
        else {
            log_warning("Failed to notify peer %s of error state.", peer->addr_str);
            ret = CONN_CLOSED;
        }
        goto end;
    }
    log_debug("Successful parsing of chunk.");

    if (peer->parser.stage == PROTO_MSG_STAGE_NONE) {
        log_info("Got msg %s from peer %s.",
                 lookup_by_id(proto_msg_type_names, peer->parser.msg_type),
                 peer->addr_str);
        // TODO: call tcp handlers here.
    }

  end:
    return ret;
}

bool peer_conn_close(struct peer *peer)
{
    bool ret = true;
    log_info("Closing connection with peer %s.", peer->addr_str);
    if (!sock_close(peer->fd)) {
        log_perror(LOG_ERR, "Failed closed for peer: %s.", errno);
        ret = false;
    }
    peer_unregister(peer);
    return ret;
}

int peer_conn_close_all(struct list_item *peers)
{
    int fail = 0;
    while (!list_is_empty(peers)) {
        struct peer *p = cont(peers->prev, struct peer, item);
        if (!peer_conn_close(p))
            fail++;
    }
    return fail;
}

/* « To join the network, a node u must have a contact to an already
   participating node w. u inserts w into the appropriate k-bucket. u then
   performs a node lookup for its own node ID. Finally, u refreshes all
   k-buckets further away than its closest neighbor. »

   We'll stick to kad's spec as a first approach.  Note bittorrent has a
   slightly different approach: only ip/port's without guid are used for
   bootstrap nodes. In bittorrent original kad implementation, bootstrap nodes
   are then ping'd and then added to the routes. In recent implementations,
   router nodes are handled differently than normal nodes.
*/
static bool kad_lookup_start(const kad_guid target, struct kad_ctx *ctx);
bool kad_bootstrap(const struct config *conf, struct kad_ctx *kctx)
{
    char bootstrap_nodes_path[PATH_MAX];
    bool found = false;
    const char *elts[] = {conf->conf_dir, DATADIR, NULL};
    const char **it = elts;
    while (*it) {
        int rv = snprintf(bootstrap_nodes_path, PATH_MAX-1, "%s/"BOOTSTRAP_FILENAME, *it);
        if (rv < 0) {
            log_error("Failed snprintf.");
            return false;
        }
        bootstrap_nodes_path[PATH_MAX-1] = '\0';
        if (access(bootstrap_nodes_path, R_OK|W_OK) != -1) {
            found = true;
            break;
        }
        it++;
    }

    if (!found) {
        log_warning("Bootstrap node file not readable and writable.");
        return true;
    }

    struct kad_node_info nodes[BOOTSTRAP_NODES_LEN];
    int nodes_len = kad_read_bootstrap_nodes(nodes, ARRAY_LEN(nodes), bootstrap_nodes_path);
    if (nodes_len < 0) {
        log_error("Failed to read bootstrap nodes.");
        return false;
    }
    log_info("%d bootstrap nodes read.", nodes_len);
    if (nodes_len == 0) {
        log_warning("No bootstrap nodes read.");
        return true;
    }

    for (int i = 0; i < nodes_len; ++i) {
        if (!routes_insert(kctx->routes, &nodes[i], 0))
            log_error("Could not insert bootstrap node to routes.");
    }

    return kad_lookup_start(kctx->routes->self_id, kctx);
}

bool kad_query(struct kad_ctx *kctx,
               const struct kad_node_info node,
               const struct kad_rpc_msg msg)
{
    struct kad_rpc_query *query = calloc(1, sizeof(struct kad_rpc_query));
    if (!query) {
        log_perror(LOG_ERR, "Failed malloc: %s.", errno);
        return false;
    }
    memcpy(&query->node, &node, sizeof(struct kad_node_info));
    memcpy(&query->msg, &msg, sizeof(struct kad_rpc_msg));

    struct iobuf qbuf = {0};
    if (!kad_rpc_query_create(&qbuf, query, kctx)) {
        goto failed;
    }

    LOG_FMT_HEX_DECL(tx_id, KAD_RPC_MSG_TX_ID_LEN);
    log_fmt_hex(tx_id, KAD_RPC_MSG_TX_ID_LEN, query->msg.tx_id.bytes);
    log_info("Sending kad msg [%d] to %s (id=%s)", query->msg.meth, node.addr_str, tx_id);

    socklen_t addr_len = sizeof(struct sockaddr_storage);
    ssize_t slen = sendto(kctx->sock, qbuf.buf, qbuf.pos, 0, (struct sockaddr *)&node.addr, addr_len);
    if (slen < 0) {
        if (errno != EWOULDBLOCK) {
            log_perror(LOG_ERR, "Failed sendto: %s", errno);
        }
        goto failed;
    }
    log_debug("Sent %d bytes.", slen);
    iobuf_reset(&qbuf);

    struct kad_rpc_query *evicted = NULL;
    if (!req_lru_put(kctx->reqs_out, query, &evicted)) {
        log_error("Cannot register duplicate query (id=%s)", tx_id);
        goto failed;
    }
    if (evicted) {
        long long now = now_millis();
        if (now < 0)
            goto failed;
        if (evicted->created + KAD_RPC_QUERY_TIMEOUT_MILLIS < now)
            routes_mark_stale(kctx->routes, &evicted->node.id);
        log_info("Evicted query from full list.");
    }

    bool is_lookup_query = query->msg.meth == KAD_RPC_METH_FIND_NODE;
    if (is_lookup_query && !kad_lookup_par_add(&kctx->lookup, query))
        log_error("Already %d find_node requests in-flight.", kctx->lookup.par_len);

    iobuf_reset(&qbuf);
    return true;

  failed:
    iobuf_reset(&qbuf);
    free_safer(query);
    return false;
}

bool kad_ping(struct kad_ctx *kctx, const struct kad_node_info node)
{
    struct kad_rpc_msg msg = {
        .meth=KAD_RPC_METH_PING
    };
    return kad_query(kctx, node, msg);
}


bool kad_find_node(struct kad_ctx *kctx, const struct kad_node_info node,
                   const kad_guid target)
{
    struct kad_rpc_msg msg = {
        .meth=KAD_RPC_METH_FIND_NODE,
        .target=target
    };
    return kad_query(kctx, node, msg);
}

static bool kad_schedule_find_nodes(
    const kad_guid target,
    struct kad_node_info nodes[], size_t nodes_len,
    struct kad_ctx *kctx)
{
    long long now = now_millis();
    if (now < 0)
        return false;

    struct list_item timers_tmp;
    list_init(&timers_tmp);

    struct event *events[nodes_len];
    size_t i=0;
    for (; i<nodes_len; i++) {
        events[i] = malloc(sizeof(struct event));
        if (!events[i]) {
            log_perror(LOG_ERR, "Failed malloc: %s.", errno);
            goto cleanup;
        }
        *events[i] = (struct event){
            "kad-find-node", .cb=event_kad_find_node_cb,
            .args.kad_find_node={.target=target, .node=nodes[i], .kctx=kctx},
            .fatal=false, .self=events[i]
        };
        sockaddr_storage_fmt(events[i]->args.kad_find_node.node.addr_str,
                             &events[i]->args.kad_find_node.node.addr);

        struct timer *timer = malloc(sizeof(struct timer));
        if (!timer) {
            log_perror(LOG_ERR, "Failed malloc: %s.", errno);
            goto cleanup;
        }
        *timer = (struct timer){
            .name="kad-find-node", .once=true,
            .delay=0 /* not speading the queries for now */,
            .event=events[i], .self=timer
        };
        timer_init(&timers_tmp, timer, now);
    }

    list_concat(kctx->timers, &timers_tmp);
    return true;

  cleanup:
    for (size_t j=0; j<i; j++)
        free(events[j]);
    list_free_all((&timers_tmp), struct timer, item);
    return false;
}

static bool kad_schedule_timeout(const int round, struct kad_ctx *ctx)
{
    long long now = now_millis();
    if (now < 0)
        return false;

    struct event *evt = malloc(sizeof(struct event));
    if (!evt) {
        log_perror(LOG_ERR, "Failed malloc: %s.", errno);
        goto cleanup;
    }
    *evt = (struct event){
        "kad-lookup", .cb=event_kad_lookup_cb,
        .args.kad_lookup={.round=round, .kctx=ctx},
        .fatal=false, .self=evt
    };

    if (!set_timeout(ctx->timers, KAD_LOOKUP_TIMEOUT_MILLIS, true, evt))
        goto cleanup;

    return true;

  cleanup:
    free_safer(evt);
    return false;
}

/* https://blog.libtorrent.org/2014/11/dht-routing-table-maintenance/ */
bool kad_refresh(void *data)
{
    (void)data;
    log_info("FIXME refresh cb");
    return true;
}

void kad_lookup_complete(struct kad_ctx *ctx)
{
    // TODO return the k closest nodes to target from lookup.past
    kad_lookup_reset(&ctx->lookup);
    log_debug("Lookup complete.");
}

static void
lookup_past_insert(struct kad_ctx *ctx,
                   struct kad_node_lookup *contacted[], size_t contacted_len)
{
    for (size_t i = 0; i < contacted_len; ++i)
        if (!node_heap_insert(&ctx->lookup.past, contacted[i])) {
            log_error("Failed insert into lookup past nodes.");
            free_safer(contacted[i]);
        }
}

static bool
kad_lookup_send(struct kad_node_info next[], size_t next_len,
                const kad_guid target, struct kad_ctx *ctx)
{
    // FIXME in kad_lookup_recv() ?
    if (ctx->lookup.round >= KAD_K_CONST) {
        kad_lookup_complete(ctx);
        return true;
    }

    if (next_len == 0) {
        log_debug("Lookup nodes exhausted.");
        kad_lookup_complete(ctx);
        return true;
    }

    log_debug("Scheduling %d find_node lookups.", next_len);
    if (!kad_schedule_find_nodes(target, next, next_len, ctx))
        return false;

    return kad_schedule_timeout(ctx->lookup.round, ctx);
}

static bool kad_lookup_start(const kad_guid target, struct kad_ctx *ctx)
{
    log_debug("Lookup start, round=%d", ctx->lookup.round);
    struct kad_node_info next[KAD_K_CONST] = {0};
    size_t next_len = 0;

    if (!kad_lookup_par_is_empty(&ctx->lookup)) {
        log_error("Lookup start: in-flight list not empty.");
        return false;
    }

    next_len = routes_find_closest(ctx->routes, &ctx->routes->self_id,
                                   next, NULL);
    if (next_len > KAD_ALPHA_CONST)
        next_len = KAD_ALPHA_CONST;

    struct kad_node_lookup *contacted[KAD_K_CONST] = {0};
    for (size_t i = 0; i < next_len; ++i)
        contacted[i] = kad_lookup_new_from(&next[i], target);

    lookup_past_insert(ctx, contacted, next_len);

    return kad_lookup_send(next, next_len, target, ctx);
}

bool kad_lookup_next(const kad_guid target, struct kad_ctx *ctx)
{
    log_debug("Lookup send, round=%d", ctx->lookup.round);
    struct kad_node_info next[KAD_K_CONST] = {0};
    size_t next_len = 0;

    struct kad_node_lookup *contacted[KAD_K_CONST] = {0};
    for (size_t i = 0; i < ctx->lookup.par_len; ++i) {
        struct kad_rpc_query *query = ctx->lookup.par[i];
        if (query != NULL) {
            long long now = now_millis();
            if (now >= 0 && query->created + KAD_RPC_QUERY_TIMEOUT_MILLIS < now) {
                routes_mark_stale(ctx->routes, &query->node.id);
                free_safer(query);
                query = NULL;
            }
            continue;
        }

        struct kad_node_lookup *nl = node_heap_get(&ctx->lookup.next);
        if (!nl)
            continue;

        next[next_len].id = nl->id;
        next[next_len].addr = nl->addr;

        contacted[next_len] = nl;

        next_len++;
    }

    lookup_past_insert(ctx, contacted, next_len);

    return kad_lookup_send(next, next_len, target, ctx);
}

static void lookup_par_discard(struct kad_ctx *ctx)
{
    for (size_t i = 0; i < KAD_K_CONST; ++i) {
        struct kad_rpc_query *query = ctx->lookup.par[i];
        if (!query)
            continue;
        if (!req_lru_delete(ctx->reqs_out, query->msg.tx_id, &query)) {
            LOG_FMT_HEX_DECL(tx_id, KAD_RPC_MSG_TX_ID_LEN);
            log_fmt_hex(tx_id, KAD_RPC_MSG_TX_ID_LEN, query->msg.tx_id.bytes);
            log_error("In-flight query (tx_id=%s) not found in request list.", tx_id);
        }
        free_safer(query);
        ctx->lookup.par[i] = NULL;
    }
}

bool kad_lookup_timeout(const int round, struct kad_ctx *ctx)
{
    if (round < ctx->lookup.round)
        return true;

    if (round > ctx->lookup.round) {
        log_error("Lookup timeout for round %d triggered during prior round %d.", round, ctx->lookup.round);
        return false;
    }

    if (kad_lookup_par_is_empty(&ctx->lookup)) {
        log_debug("Lookup timeout for round %d: no queries in flight.", round);
        return true;
    }

    lookup_par_discard(ctx);

    return kad_lookup_start(ctx->lookup.past.items[0]->target, ctx);

}
