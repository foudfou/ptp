/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include "log.h"
#include "net/kad/bencode/parser.h"
#include "net/kad/bencode/serde.h"
#include "utils/array.h"
#include "net/kad/bencode/rpc_msg.h"

#define KAD_RPC_MSG_LITERAL_MAX 10 + KAD_K_CONST
#define KAD_RPC_MSG_NODES_MAX   32

enum kad_rpc_msg_key {
    KAD_RPC_MSG_KEY_NONE,
    KAD_RPC_MSG_KEY_TX_ID,
    KAD_RPC_MSG_KEY_NODE_ID,
    KAD_RPC_MSG_KEY_TYPE,
    KAD_RPC_MSG_KEY_METH,
    KAD_RPC_MSG_KEY_ERROR,
    KAD_RPC_MSG_KEY_ARG,
    KAD_RPC_MSG_KEY_RES,
    KAD_RPC_MSG_KEY_TARGET,
    KAD_RPC_MSG_KEY_NODES,
};

static const lookup_entry kad_rpc_msg_key_names[] = {
    { KAD_RPC_MSG_KEY_TX_ID,    "t" },
    { KAD_RPC_MSG_KEY_NODE_ID,  "id" },
    { KAD_RPC_MSG_KEY_TYPE,     "y" },
    { KAD_RPC_MSG_KEY_METH,     "q" },
    { KAD_RPC_MSG_KEY_ERROR,    "e" },
    { KAD_RPC_MSG_KEY_ARG,      "a" },
    { KAD_RPC_MSG_KEY_RES,      "r" },
    { KAD_RPC_MSG_KEY_TARGET,   "target" },
    { KAD_RPC_MSG_KEY_NODES,    "nodes" },
    { 0,                          NULL },
};

static bool
benc_read_rpc_msg_tx_id(kad_rpc_msg_tx_id *id, const struct benc_literal *lit)
{
    if (lit->t != BENC_LITERAL_TYPE_STR) {
        log_error("Message node id not a string.");
        return false;
    }
    if (lit->s.len != KAD_RPC_MSG_TX_ID_LEN) {
        log_error("Message tx id has wrong length (%zu).", lit->s.len);
        return false;
    }
    kad_rpc_msg_tx_id_set(id, (unsigned char*)lit->s.p);
    return true;
}

static enum kad_rpc_meth
benc_get_rpc_msg_meth(const struct benc_node *dict)
{
    const char *key = lookup_by_id(kad_rpc_msg_key_names, KAD_RPC_MSG_KEY_METH);
    const struct benc_node *n = benc_node_find_literal_str(dict, key, 1);
    if (!n) {
        return KAD_RPC_METH_NONE;
    }
    return lookup_by_name(kad_rpc_meth_names, n->chd[0]->lit->s.p, 10);
}

static bool
benc_read_guid_from_key(kad_guid *guid,
                        const struct benc_node *dict,
                        const enum kad_rpc_msg_key k1,
                        const enum kad_rpc_msg_key k2)
{
    const struct benc_node *n = benc_node_navigate_to_key(dict, kad_rpc_msg_key_names, k1, k2);
    if (!n) {
        return false;
    }
    if (!benc_read_guid(guid, n->chd[0]->lit)) {
        log_error("Node_id copy failed.");
        return false;
    }
    return true;
}

/**
 * Parses @buf and populates @msg accordingly
 *
 * "t" transaction id: 2 chars.
 * "y" message type: "q" for query, "r" for response, or "e" for error.
 *
 * "q" query method name: str.
 * "a" named arguments: dict.
 * "r" named return values: dict.
 *
 * "e" list: error code (int), error msg (str).
 *
 * Note about node info encoding: we embrace the BitTorrent spec where a
 * "Compact node info" is a 26-byte string (20-byte node-id + 6-byte "Compact
 * IP-address/port info" (4-byte IP (16-byte for ip6) + 2-byte port all in
 * network byte order))".
 */
bool benc_decode_rpc_msg(struct kad_rpc_msg *msg, const char buf[], const size_t slen) {
    benc_repr_init();

    if (!benc_parse(&repr, buf, slen)) {
        return false;
    }

    // is_dict
    if (repr.n[0].typ != BENC_NODE_TYPE_DICT) {
        log_error("Decoded bencode object not a dict.");
        return false;
    }

    const char *key = lookup_by_id(kad_rpc_msg_key_names, KAD_RPC_MSG_KEY_TX_ID);
    const struct benc_node *n = benc_node_find_literal_str(&repr.n[0], key, 1);
    if (!n || n->chd[0]->lit->s.len != 2) {
        return false;
    }
    if (!benc_read_rpc_msg_tx_id(&msg->tx_id, n->chd[0]->lit)) {
        log_error("Tx_id copy failed.");
        return false;
    }

    key = lookup_by_id(kad_rpc_msg_key_names, KAD_RPC_MSG_KEY_TYPE);
    n = benc_node_find_literal_str(&repr.n[0], key, 1);
    if (!n || n->chd[0]->lit->s.len != 1) {
        return false;
    }
    msg->type = lookup_by_name(kad_rpc_type_names, n->chd[0]->lit->s.p, 1);
    if (msg->type == KAD_RPC_TYPE_NONE) {
        log_error("Unknown message type '%c'.", n->chd[0]->lit->s.p);
        return false;
    }

    switch (msg->type) {
    case KAD_RPC_TYPE_ERROR: {
        key = lookup_by_id(kad_rpc_msg_key_names, KAD_RPC_MSG_KEY_ERROR);
        n = benc_node_find_key(&repr.n[0], key, 1);
        if (!n) {
            log_error("Missing entry (%s) in decoded bencode object.", key);
            return false;
        }
        if (n->chd[0]->typ != BENC_NODE_TYPE_LIST) {
            log_error("Invalid entry %s.", key);
            return false;
        }
        n = n->chd[0];
        if (n->chd[0]->typ != BENC_NODE_TYPE_LITERAL ||
            n->chd[0]->lit->t != BENC_LITERAL_TYPE_INT) {
            log_error("Invalid value type for elt[0] of %s.", key);
            return false;
        }
        msg->err_code = n->chd[0]->lit->i;
        if (n->chd[1]->typ != BENC_NODE_TYPE_LITERAL ||
            n->chd[1]->lit->t != BENC_LITERAL_TYPE_STR) {
            log_error("Invalid value type for elt[0] of %s.", key);
            return false;
        }
        memcpy(msg->err_msg, n->chd[1]->lit->s.p, n->chd[1]->lit->s.len);
        break;
    }

    case KAD_RPC_TYPE_QUERY: {
        msg->meth = benc_get_rpc_msg_meth(&repr.n[0]);
        if (msg->meth == KAD_RPC_METH_NONE) {
            log_error("Unknown message method '%c'.", n->chd[0]->lit->s.p);
            return false;
        }

        if (msg->meth == KAD_RPC_METH_PING) {
            // get "a":{"id":"abcdefghij0123456789"}
            if (!benc_read_guid_from_key(&msg->node_id, &repr.n[0], KAD_RPC_MSG_KEY_ARG, KAD_RPC_MSG_KEY_NODE_ID)) {
                return false;
            }
        }

        else if (msg->meth == KAD_RPC_METH_FIND_NODE) {
            if (!benc_read_guid_from_key(&msg->node_id, &repr.n[0], KAD_RPC_MSG_KEY_ARG, KAD_RPC_MSG_KEY_NODE_ID)) {
                return false;
            }
            if (!benc_read_guid_from_key(&msg->target, &repr.n[0], KAD_RPC_MSG_KEY_ARG, KAD_RPC_MSG_KEY_TARGET)) {
                return false;
            }
        }

        else {
            // Should never happen as we've already looked up the msg type.
            log_error("Unknown message type '%d'.", msg->type);
            return false;
        }

        break;
    }

    case KAD_RPC_TYPE_RESPONSE: {
        // Responses do not have any method name.

        // get "r":{"id":"abcdefghij0123456789"}
        if (!benc_read_guid_from_key(&msg->node_id, &repr.n[0], KAD_RPC_MSG_KEY_RES, KAD_RPC_MSG_KEY_NODE_ID)) {
            return false;
        }

        // attempt to get nodes in case we're in a find_node response
        /* NOTE the protocol says « a string containing the compact node info
           for the target node or the K (8) closest good nodes ». For now
           we're always expecting/giving a list.  */
        int nnodes = benc_read_nodes_from_key(msg->nodes, ARRAY_LEN(msg->nodes), &repr.n[0],
                                              kad_rpc_msg_key_names, KAD_RPC_MSG_KEY_RES, KAD_RPC_MSG_KEY_NODES);
        if (nnodes > 0) {
            msg->nodes_len = nnodes;
        }

        break;
    }

    default:
        log_error("Unknown msg type '%d'.", msg->type);
        return false;

        break;
    }


    return true;
}

/**
 * Straight-forward serialization. NO VALIDATION is performed.
 */
// FIXME dict entries supposed to be sorted when serialized. Basically we just
// need to reverse the current order.
bool benc_encode_rpc_msg(struct iobuf *buf, const struct kad_rpc_msg *msg)
{
    char tmps[2048];
    size_t tmps_len = 0;

    /* we avoid the burden of looking up into kad_rpc_msg_key_names just for single chars. */
    sprintf(tmps, "d1:t%d:", KAD_RPC_MSG_TX_ID_LEN);
    tmps_len += strlen(tmps);
    memcpy(tmps + tmps_len, msg->tx_id.bytes, KAD_RPC_MSG_TX_ID_LEN);
    tmps_len += KAD_RPC_MSG_TX_ID_LEN;
    iobuf_append(buf, tmps, tmps_len); // tx
    iobuf_append(buf, "1:y1:", 5); // type
    iobuf_append(buf, lookup_by_id(kad_rpc_type_names, msg->type), 1);

    if (msg->type == KAD_RPC_TYPE_ERROR) {
        sprintf(tmps, "1:eli%llue%zu:%se", msg->err_code,
                strlen(msg->err_msg), msg->err_msg);
        iobuf_append(buf, tmps, strlen(tmps));
    }
    else {

        if (msg->type == KAD_RPC_TYPE_QUERY) {
            const char * meth_name = lookup_by_id(kad_rpc_meth_names, msg->meth);
            sprintf(tmps, "1:q%zu:%s", strlen(meth_name), meth_name);
            iobuf_append(buf, tmps, strlen(tmps));

            if (msg->meth == KAD_RPC_METH_PING) {
                iobuf_append(buf, "1:ad2:id", 8);
            }
            else if (msg->meth == KAD_RPC_METH_FIND_NODE) {
                const char *field_target = lookup_by_id(
                    kad_rpc_msg_key_names, KAD_RPC_MSG_KEY_TARGET);
                sprintf(tmps, "1:ad%zu:%s%d:", strlen(field_target),
                        field_target, KAD_GUID_SPACE_IN_BYTES);
                tmps_len = strlen(tmps);
                memcpy(tmps + tmps_len, (char*)msg->target.bytes,
                       KAD_GUID_SPACE_IN_BYTES);
                tmps_len += KAD_GUID_SPACE_IN_BYTES;
                memcpy(tmps + tmps_len, "2:id", 4);
                tmps_len += 4;
                iobuf_append(buf, tmps, tmps_len); // target
            }
            else {
                log_error("Unsupported msg method while encoding.");
                return false;
            }

        }
        else if (msg->type == KAD_RPC_TYPE_RESPONSE) {

            if (msg->meth == KAD_RPC_METH_PING) {
                iobuf_append(buf, "1:rd2:id", 8);
            }
            else if (msg->meth == KAD_RPC_METH_FIND_NODE) {
                const char *field_nodes = lookup_by_id(
                    kad_rpc_msg_key_names, KAD_RPC_MSG_KEY_NODES);
                sprintf(tmps, "1:rd%zu:%sl", strlen(field_nodes), field_nodes);
                iobuf_append(buf, tmps, strlen(tmps));
                if (!benc_write_nodes(buf, msg->nodes, msg->nodes_len)) {
                    return false;
                }
                iobuf_append(buf, "e2:id", 5);
            }
            else {
                log_error("Unsupported msg method while encoding.");
                return false;
            }

        }
        else {
            log_error("Unsupported msg type while encoding.");
            return false;
        }

        sprintf(tmps, "%d:", KAD_GUID_SPACE_IN_BYTES);
        tmps_len = strlen(tmps);
        memcpy(tmps + tmps_len, (char*)msg->node_id.bytes,
               KAD_GUID_SPACE_IN_BYTES);
        tmps_len += KAD_GUID_SPACE_IN_BYTES;
        iobuf_append(buf, tmps, tmps_len); // node_id

        iobuf_append(buf, "e", 1);
    }

    iobuf_append(buf, "e", 1);
    return true;
}
