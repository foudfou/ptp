/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <limits.h>
#include <unistd.h>
#include "log.h"
#include "net/kad/bencode/rpc_msg.h"
#include "net/kad/req_lru.h"
#include "net/socket.h"
#include "timers.h"
#include "utils/safer.h"
#include "utils/time.h"
#include "net/kad/rpc.h"

#define ROUTES_STATE_FILENAME "routes.dat"

/**
 * Creates routes.
 *
 * If @conf_dir is provied, then read routes from routes file. Otherwise create
 * new routes.
 *
 * Returns the number of known nodes or -1 in case of failure.
 */
int kad_rpc_init(struct kad_ctx *ctx, const char conf_dir[])
{
    rand_init();

    int nodes_len = 0;
    if (conf_dir) {
        char routes_state_path[PATH_MAX];
        int rv = snprintf(routes_state_path, PATH_MAX-1, "%s/"ROUTES_STATE_FILENAME, conf_dir);
        if (rv < 0) {
            log_error("Failed snprintf.");
            return false;
        }
        routes_state_path[PATH_MAX-1] = '\0';
        if (access(routes_state_path, R_OK|W_OK) != -1 ) {
            nodes_len = routes_read(&ctx->routes, routes_state_path);
        } else {
            log_info("Routes state file not readable and writable. Generating new routes.");
            ctx->routes = routes_create();
        }
    }
    else {
        ctx->routes = routes_create();
    }

    if (!ctx->routes) {
        log_error("Could not initialize routes.");
        return -1;
    }

    req_lru_init(ctx->reqs_out);
    node_heap_init(&ctx->lookup.nodes, 32);

    log_debug("Routes initialized.");
    return nodes_len;
}

void kad_rpc_terminate(struct kad_ctx *ctx, const char conf_dir[])
{
    if (conf_dir) {
        char routes_state_path[PATH_MAX];
        snprintf(routes_state_path, PATH_MAX-1, "%s/"ROUTES_STATE_FILENAME, conf_dir);
        routes_state_path[PATH_MAX-1] = '\0';
        if (!routes_write(ctx->routes, routes_state_path)) {
            log_error("Saving routes failed.");
        }
    }

    routes_destroy(ctx->routes);

    free_safer(ctx->lookup.nodes.items);

    struct list_item *reqs_out = &ctx->reqs_out->litems;
    list_free_all(reqs_out, struct kad_rpc_query, litem);
    log_debug("Routes terminated.");
}

static bool
kad_rpc_update_routes(struct kad_ctx *ctx, const struct sockaddr_storage *addr,
                   const kad_guid *node_id)
{
    time_t now = 0;
    if (!now_sec(&now))
        return false;

    bool rv = true;
    struct kad_node_info info = {.id=*node_id, .addr=*addr};
    sockaddr_storage_fmt(info.addr_str, addr);
    char *id = log_fmt_hex(LOG_DEBUG, node_id->bytes, KAD_GUID_SPACE_IN_BYTES);
    if ((rv = routes_update(ctx->routes, &info, now)))
        log_debug("Routes update of %s (id=%s).", &info.addr_str, id);
    else if ((rv = routes_insert(ctx->routes, &info, now)))
        log_debug("Routes insert of %s (id=%s).", &info.addr_str, id);
    else
        log_warning("Failed to upsert kad_node (id=%s)", id);
    free_safer(id);

    return rv;
}

static bool kad_rpc_handle_error(const struct kad_rpc_msg *msg)
{
    char *id = log_fmt_hex(LOG_DEBUG, msg->node_id.bytes, KAD_GUID_SPACE_IN_BYTES);
    log_error("Received error message (%zull) from id(%s): %s.",
              msg->err_code, id, msg->err_msg);
    free_safer(id);
    return true;
}

static bool
kad_rpc_handle_query(struct kad_ctx *ctx, const struct kad_rpc_msg *msg,
                     struct iobuf *rsp)
{
    switch (msg->meth) {
    case KAD_RPC_METH_NONE: {
        log_error("Got query for method none.");
        return false;
    }

    case KAD_RPC_METH_PING: {
        struct kad_rpc_msg resp = {0};
        resp.tx_id = msg->tx_id;
        resp.node_id = ctx->routes->self_id;
        resp.type = KAD_RPC_TYPE_RESPONSE;
        resp.meth = KAD_RPC_METH_PING;
        if (!benc_encode_rpc_msg(rsp, &resp)) {
            log_error("Error while encoding ping response.");
            return false;
        }
        break;
    }

    case KAD_RPC_METH_FIND_NODE: {
        struct kad_rpc_msg resp = {0};
        resp.tx_id = msg->tx_id;
        resp.node_id = ctx->routes->self_id;
        resp.type = KAD_RPC_TYPE_RESPONSE;
        resp.meth = KAD_RPC_METH_FIND_NODE;
        resp.nodes_len = routes_find_closest(ctx->routes, &msg->target, resp.nodes,
                                          &msg->node_id);
        if (!benc_encode_rpc_msg(rsp, &resp)) {
            log_error("Error while encoding find node response.");
            return false;
        }
        break;
    }

    default:
        break;
    }
    return true;
}

static bool
kad_rpc_handle_response(struct kad_ctx *ctx, const struct kad_rpc_msg *msg)
{
    bool ret = false;

    char *id = log_fmt_hex(LOG_DEBUG, msg->tx_id.bytes, KAD_RPC_MSG_TX_ID_LEN);

    struct kad_rpc_query *query = NULL;
    if (!req_lru_delete(ctx->reqs_out, msg->tx_id, &query)) {
        log_warning("Query for response (id=%s) not found.", id);
        goto cleanup;
    }

    switch (query->msg.meth) {
    case KAD_RPC_METH_NONE: {
        log_error("Got query for method none.");
        goto cleanup;
    }

    case KAD_RPC_METH_PING: {
        log_debug("Handling ping response (id=%s).", id);
        // Nothing left to do as routes already updated in kad_rpc_handle
        break;
    }

    case KAD_RPC_METH_FIND_NODE: {
        for (size_t i=0; i<msg->nodes_len; ++i) {
            if (!msg->nodes[i].id.is_set) {
                log_warning("Node id not set, routes not updated.");
                continue;
            }

            struct kad_node_info info = {.id=msg->nodes[i].id, .addr=msg->nodes[i].addr};
            sockaddr_storage_fmt(info.addr_str, &info.addr);
            if (routes_insert(ctx->routes, &info, 0))
                log_warning("Ignoring failed routes insert.");
        }
        break;
    }

    default:
        break;
    }

    free_safer(query);
    ret = true;

  cleanup:
    free_safer(id);
    return ret;
}

static void kad_rpc_generate_tx_id(kad_rpc_msg_tx_id *tx_id)
{
    unsigned char rand[KAD_RPC_MSG_TX_ID_LEN];
    for (int i = 0; i < KAD_RPC_MSG_TX_ID_LEN; i++)
        rand[i] = (unsigned char)random();
    kad_rpc_msg_tx_id_set(tx_id, rand);
}

static void
kad_rpc_error(struct kad_rpc_msg *out, const enum kad_rpc_err err,
              const struct kad_rpc_msg *in, const kad_guid *self_id)
{
    if (in->tx_id.is_set)
        out->tx_id = in->tx_id;
    else
        kad_rpc_generate_tx_id(&out->tx_id); // TODO: track this tx ?
    out->node_id = *self_id;
    out->type = KAD_RPC_TYPE_ERROR;
    out->err_code = err;
    strcpy(out->err_msg, lookup_by_id(kad_rpc_err_names, err));
}

/**
 * Processes the incoming message in `buf` and places the response, if any,
 * into the provided `rsp` buffer.
 */
bool kad_rpc_handle(struct kad_ctx *ctx, const struct sockaddr_storage *addr,
                    const char buf[], const size_t slen, struct iobuf *rsp)
{
    struct kad_rpc_msg msg = {0};

    if (!benc_decode_rpc_msg(&msg, buf, slen)) {
        log_error("Invalid message received.");
        struct kad_rpc_msg rspmsg = {0};
        kad_rpc_error(&rspmsg, KAD_RPC_ERR_PROTOCOL, &msg, &ctx->routes->self_id);
        if (!benc_encode_rpc_msg(rsp, &rspmsg))
            log_error("Error while encoding error response.");
        return false;
    }
    kad_rpc_msg_log(&msg); // TESTING

    if (msg.node_id.is_set && !kad_rpc_update_routes(ctx, addr, &msg.node_id))
        log_warning("Routes update failed.");

    switch (msg.type) {
    case KAD_RPC_TYPE_NONE: {
        log_error("Got msg of type none.");
        return false;
    }

    case KAD_RPC_TYPE_ERROR: {
        return kad_rpc_handle_error(&msg);
    }

    case KAD_RPC_TYPE_QUERY: {  /* We'll respond immediately */
        return kad_rpc_handle_query(ctx, &msg, rsp);
    }

    case KAD_RPC_TYPE_RESPONSE: {
        return kad_rpc_handle_response(ctx, &msg);
    }

    default:
        log_error("Unknown msg type.");
        return false;
    }
}

/**
 * For debugging only !
 */
void kad_rpc_msg_log(const struct kad_rpc_msg *msg)
{
    char *tx_id = log_fmt_hex(LOG_DEBUG, msg->tx_id.bytes, KAD_RPC_MSG_TX_ID_LEN);
    char *node_id = log_fmt_hex(LOG_DEBUG, msg->node_id.bytes,
                                KAD_GUID_SPACE_IN_BYTES);
    log_debug(
        "msg={\n  tx_id=0x%s\n  node_id=0x%s\n  type=%d\n  err_code=%lld\n"
        "  err_msg=%s\n  meth=%d",
        tx_id, node_id, msg->type, msg->err_code, msg->err_msg,
        msg->meth, msg->target);
    free_safer(tx_id);
    free_safer(node_id);

    node_id = log_fmt_hex(LOG_DEBUG, msg->target.bytes,
                          msg->target.is_set ? KAD_GUID_SPACE_IN_BYTES : 0);
    log_debug("  target=0x%s", node_id);
    free_safer(node_id);

    for (size_t i = 0; i < msg->nodes_len; i++) {
        node_id = log_fmt_hex(LOG_DEBUG, msg->nodes[i].id.bytes,
                              KAD_GUID_SPACE_IN_BYTES);
        log_debug("  nodes[%zu]=0x%s %s", i, node_id, msg->nodes[i].addr_str);
        free_safer(node_id);
    }
    log_debug("}");
}

bool kad_rpc_query_create(struct iobuf *buf,
                          struct kad_rpc_query *query,
                          const struct kad_ctx *ctx)
{
    list_init(&query->litem);
    if ((query->created = now_millis()) == -1)
        return false;
    kad_rpc_generate_tx_id(&query->msg.tx_id);
    query->msg.node_id = ctx->routes->self_id;
    query->msg.type = KAD_RPC_TYPE_QUERY;

    if (!benc_encode_rpc_msg(buf, &query->msg)) {
        log_error("Error while encoding ping query.");
        return false;
    }
    return true;
}
