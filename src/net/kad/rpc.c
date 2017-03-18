/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include "log.h"
#include "net/kad/bencode.h"
#include "utils/safer.h"
#include "net/kad/rpc.h"

bool kad_rpc_init(struct kad_ctx *ctx)
{
    ctx->dht = kad_dht_init();
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
    kad_dht_terminate(ctx->dht);
    // TODO: terminate queries
    log_debug("DHT terminated.");
}

bool kad_rpc_msg_validate(const struct kad_rpc_msg *msg)
{
    (void)msg; // FIXME:
    return true;  /* TODO: should we validate beforehand ? */
}

bool kad_rpc_handle(struct kad_ctx *ctx,
                    const char host[], const char service[],
                    const char buf[], const size_t slen)
{
    (void)ctx; (void)host; (void)service; // FIXME:
    struct kad_rpc_msg msg = {0};
    if (!benc_decode(&msg, buf, slen) ||
        !kad_rpc_msg_validate(&msg)) {
        log_error("Invalid message.");
        return false;
    }
    kad_rpc_msg_log(&msg);

    switch (msg.type) {
    case KAD_RPC_TYPE_NONE: {
        log_debug("Got msg none");
        break;
    }

    case KAD_RPC_TYPE_QUERY: {
        log_debug("Got msg query");
        break;
    }

    case KAD_RPC_TYPE_RESPONSE: {
// we should probably keep track of our queries in a list of sent msgs in ctx…
        log_debug("Got msg response");
        break;
    }

    default:
        break;
    }

    return true;
}

/**
 * Use only for debugging !
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
