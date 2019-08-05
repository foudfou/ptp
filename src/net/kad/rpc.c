/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include "log.h"
#include "net/kad/bencode.h"
#include "utils/safer.h"
#include "net/kad/rpc.h"

bool kad_rpc_init(struct kad_ctx *ctx)
{
    ctx->dht = dht_init();
    if (!ctx->dht) {
        log_error("Could not initialize dht.");
        return false;
    }
    list_init(&ctx->queries);
    log_debug("DHT initialized.");
    return true;
}

void kad_rpc_terminate(struct kad_ctx *ctx)
{
    dht_terminate(ctx->dht);
    struct list_item *query = &ctx->queries;
    list_free_all(query, struct kad_rpc_msg, item);
    log_debug("DHT terminated.");
}

bool kad_rpc_msg_validate(const struct kad_rpc_msg *msg)
{
    (void)msg; // FIXME:
    return true;  /* TODO: should we validate beforehand ? */
}

struct kad_rpc_msg *
kad_rpc_query_find(struct kad_ctx *ctx, const kad_rpc_msg_tx_id *tx_id)
{
    struct kad_rpc_msg *m;
    struct list_item * it = &ctx->queries;
    list_for(it, &ctx->queries) {
        m = cont(it, struct kad_rpc_msg, item);
        if (memcmp(m->tx_id.bytes, tx_id->bytes, KAD_RPC_MSG_TX_ID_LEN) == 0)
            break;
    }

    if (it == &ctx->queries) {
        char *id = log_fmt_hex(LOG_DEBUG, tx_id->bytes, KAD_RPC_MSG_TX_ID_LEN);
        log_warning("Query (tx_id=%s) not found.", id);
        free_safer(id);
        return NULL;
    }

    return m;
}

static void
kad_rpc_update_dht(struct kad_ctx *ctx, const char host[], const char service[],
                   const struct kad_rpc_msg *msg)
{
    char *id = log_fmt_hex(LOG_DEBUG, msg->node_id.bytes, KAD_GUID_SPACE_IN_BYTES);
    struct kad_node_info info = {0};
    info.id = msg->node_id;
    strcpy(info.host, host);
    strcpy(info.service, service);
    int updated = dht_update(ctx->dht, &info);
    if (updated == 0)
        log_debug("DHT update of [%s]:%s (id=%s).", host, service, id);
    else if (updated > 0) { // insert needed
        if (dht_insert(ctx->dht, &info))
            log_debug("DHT insert of [%s]:%s (id=%s).", host, service, id);
        else
            log_warning("Failed to insert kad_node (id=%s).", id);
    }
    else
        log_warning("Failed to update kad_node (id=%s)", id);
    free_safer(id);
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
        struct kad_rpc_msg resp;
        KAD_RPC_MSG_INIT(resp);
        resp.tx_id = msg->tx_id;
        resp.node_id = ctx->dht->self_id;
        resp.type = KAD_RPC_TYPE_RESPONSE;
        resp.meth = KAD_RPC_METH_PING;
        if (!benc_encode(&resp, rsp)) {
            log_error("Error while encoding ping response.");
            return false;
        }
        break;
    }

    case KAD_RPC_METH_FIND_NODE: {
        struct kad_rpc_msg resp;
        KAD_RPC_MSG_INIT(resp);
        resp.tx_id = msg->tx_id;
        resp.node_id = ctx->dht->self_id;
        resp.type = KAD_RPC_TYPE_RESPONSE;
        resp.meth = KAD_RPC_METH_FIND_NODE;
        resp.nodes_len = dht_find_closest(ctx->dht, &msg->target, resp.nodes,
                                          &msg->node_id);
        if (!benc_encode(&resp, rsp)) {
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
    struct kad_rpc_msg *query = kad_rpc_query_find(ctx, &msg->tx_id);
    if (!query) {
        char *id = log_fmt_hex(LOG_DEBUG, msg->node_id.bytes,
                               KAD_GUID_SPACE_IN_BYTES);
        log_error("Query for response id(%s) not found.", id);
        free_safer(id);
        return false;
    }

    list_delete(&query->item);
    free_safer(query);
    return true;
}

static void kad_rpc_generate_tx_id(kad_rpc_msg_tx_id *tx_id)
{
    unsigned char rand[KAD_RPC_MSG_TX_ID_LEN];
    for (int i = 0; i < KAD_RPC_MSG_TX_ID_LEN; i++)
        rand[i] = (unsigned char)random();
    kad_rpc_msg_tx_id_set(tx_id, rand);
}

/**
 * Processes the incoming message in `buf` and places the response, if any,
 * into the provided `rsp` buffer.
 *
 * Returns 0 on success, -1 on failure, 1 if a response is ready.
 */
int kad_rpc_handle(struct kad_ctx *ctx, const char host[], const char service[],
                   const char buf[], const size_t slen, struct iobuf *rsp)
{
    int ret = 0;

    struct kad_rpc_msg msg;
    KAD_RPC_MSG_INIT(msg);

    if (!benc_decode(&msg, buf, slen) || !kad_rpc_msg_validate(&msg)) {
        log_error("Invalid message received.");
        struct kad_rpc_msg rspmsg;
        KAD_RPC_MSG_INIT(rspmsg);
        if (msg.tx_id.is_set)
            rspmsg.tx_id = msg.tx_id;
        else
            kad_rpc_generate_tx_id(&rspmsg.tx_id); // TODO: track this tx ?
        rspmsg.node_id = ctx->dht->self_id;
        rspmsg.type = KAD_RPC_TYPE_ERROR;
        rspmsg.err_code = KAD_RPC_ERR_PROTOCOL;
        strcpy(rspmsg.err_msg, lookup_by_id(kad_rpc_err_names,
                                            KAD_RPC_ERR_PROTOCOL));
        if (!benc_encode(&rspmsg, rsp))
            log_error("Error while encoding error response.");
        ret = -1;
        goto end;
    }
    kad_rpc_msg_log(&msg); // TESTING

    if (msg.node_id.is_set)
        kad_rpc_update_dht(ctx, host, service, &msg);
    else
        log_warning("Node id not set, DHT not updated.");

    switch (msg.type) {
    case KAD_RPC_TYPE_NONE: {
        log_error("Got msg of type none.");
        ret = -1;
        break;
    }

    case KAD_RPC_TYPE_ERROR: {
        if (!kad_rpc_handle_error(&msg))
            ret = -1;
        break;
    }

    case KAD_RPC_TYPE_QUERY: {  /* We'll respond immediately */
        ret = kad_rpc_handle_query(ctx, &msg, rsp) ? 1 : -1;
        break;
    }

    case KAD_RPC_TYPE_RESPONSE: {
        if (!kad_rpc_handle_response(ctx, &msg))
            ret = -1;
        break;
    }

    default:
        log_error("Unknown msg type.");
        break;
    }

  end:
    return ret;
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
        log_debug("  nodes[%zu]=0x%s:[%s]:%s", i, node_id,
                  msg->nodes[i].host, msg->nodes[i].service);
        free_safer(node_id);
    }
    log_debug("}");
}
