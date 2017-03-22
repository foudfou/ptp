/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <errno.h>
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
kad_rpc_query_find(struct kad_ctx *ctx, const unsigned char tx_id[])
{
    struct kad_rpc_msg *m;
    struct list_item * it = &ctx->queries;
    list_for(it, &ctx->queries) {
        m = cont(it, struct kad_rpc_msg, item);
        for (int i = 0; i < KAD_RPC_MSG_TX_ID_LEN; ++i) {
            if (m->tx_id[i] != tx_id[i])
                continue;
            break;
        }
    }

    if (it == &ctx->queries) {
        log_warning("Query (tx_id=TODO:) not found.");
        return NULL;
    }

    return m;
}

/**
 * Returns 0 on success, -1 on failure, 1 if further ping confirmation is
 * needed, in which case @ping is set.
 */
bool kad_rpc_handle(struct kad_ctx *ctx,
                    const char host[], const char service[],
                    const char buf[], const size_t slen)
{
    struct kad_rpc_msg *msg = malloc(sizeof(struct kad_rpc_msg));
    if (!msg) {
        log_perror(LOG_ERR, "Failed malloc: %s.", errno);
        return false;
    }
    memset(msg, 0, sizeof(*msg));
    list_init(&(msg->item));

    if (!benc_decode(msg, buf, slen) ||
        !kad_rpc_msg_validate(msg)) {
        log_error("Invalid message.");
        goto fail;
    }
    kad_rpc_msg_log(msg);

    char *id = log_fmt_hex(LOG_DEBUG, msg->node_id.b, KAD_GUID_BYTE_SPACE);
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

    switch (msg->type) {
    case KAD_RPC_TYPE_NONE: {
        log_error("Got msg none");
        goto fail;
    }

    case KAD_RPC_TYPE_QUERY: {  /* We'll respond immediately */
        log_debug("Got msg query");
        break;
    }

    case KAD_RPC_TYPE_RESPONSE: {
        log_debug("Got msg response");
        struct kad_rpc_msg *query = kad_rpc_query_find(ctx, msg->tx_id);
        if (!query)
            goto fail;

        list_delete(&query->item);
        free_safer(query);
        break;
    }

    default:
        break;
    }

    free_safer(msg);
    return true;
  fail:
    free_safer(msg);
    return false;
}

/**
 * For debugging only !
 */
void kad_rpc_msg_log(const struct kad_rpc_msg *msg)
{
    char *tx_id = log_fmt_hex(LOG_DEBUG, msg->tx_id, KAD_RPC_MSG_TX_ID_LEN);
    char *node_id = log_fmt_hex(LOG_DEBUG, msg->node_id.b, KAD_GUID_BYTE_SPACE);
    log_debug(
        "msg={\n  tx_id=0x%s\n  node_id=0x%s\n  type=%d\n  err_code=%lld\n"
        "  err_msg=%s\n  meth=%d",
        tx_id, node_id, msg->type, msg->err_code, msg->err_msg,
        msg->meth, msg->target);
    free_safer(tx_id);
    free_safer(node_id);

    node_id = log_fmt_hex(LOG_DEBUG, msg->target.b,
                          *msg->target.b ? KAD_GUID_BYTE_SPACE : 0);
    log_debug("  target=0x%s", node_id);
    free_safer(node_id);

    for (size_t i = 0; i < msg->nodes_len; i++) {
        node_id = log_fmt_hex(LOG_DEBUG, msg->nodes[i].id.b, KAD_GUID_BYTE_SPACE);
        log_debug("  nodes[%zu]=0x%s:[%s]:%s", i, node_id,
                  msg->nodes[i].host, msg->nodes[i].service);
        free_safer(node_id);
    }
    log_debug("}");
}
