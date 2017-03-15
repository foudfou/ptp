/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include "log.h"
#include "net/kad/bencode.h"
#include "net/kad/rpc.h"
#include "utils/safer.h"

bool kad_rpc_parse(const char host[], const char service[],
                   const char buf[], const size_t slen)
{
    (void)host; (void)service; // FIXME:
    struct kad_rpc_msg msg = {0};
    bool rv = benc_decode(&msg, buf, slen);
    kad_rpc_msg_log(&msg);
    return rv;
}

void kad_rpc_msg_log(const struct kad_rpc_msg *msg)
{
    char *tx_id = fmt_hex(msg->tx_id, KAD_RPC_MSG_TX_ID_LEN);
    char *node_id = fmt_hex(msg->node_id.b, KAD_GUID_BYTE_SPACE);
    log_debug(
        "msg={\n  tx_id=%s\n  node_id=%s\n  type=%d\n  err_code=%lld\n"
        "  err_msg=%s\n  meth=%d",
        tx_id, node_id, msg->type, msg->err_code, msg->err_msg,
        msg->meth, msg->target);
    free_safer(tx_id);
    free_safer(node_id);

    node_id = fmt_hex(msg->target.b, KAD_GUID_BYTE_SPACE);
    log_debug("  target=%s", node_id);
    free_safer(node_id);

    for (size_t i = 0; i < msg->nodes_len; i++) {
        node_id = fmt_hex(msg->nodes[i].id.b, KAD_GUID_BYTE_SPACE);
        log_debug("  %s:[%s]:%s", node_id,
                  msg->nodes[i].host, msg->nodes[i].service);
        free_safer(node_id);
    }
}
