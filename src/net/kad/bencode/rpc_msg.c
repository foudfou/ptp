#include "log.h"
#include "net/kad/bencode/parser.h"
#include "net/kad/bencode/rpc_msg.h"

enum kad_rpc_msg_field {
    KAD_RPC_MSG_FIELD_NONE,
    KAD_RPC_MSG_FIELD_TX_ID,
    KAD_RPC_MSG_FIELD_NODE_ID,
    KAD_RPC_MSG_FIELD_TYPE,
    KAD_RPC_MSG_FIELD_ERR,
    KAD_RPC_MSG_FIELD_METH,
    KAD_RPC_MSG_FIELD_TARGET,
    KAD_RPC_MSG_FIELD_NODES_ID,
    KAD_RPC_MSG_FIELD_NODES_HOST,
    KAD_RPC_MSG_FIELD_NODES_SERVICE,
};

#define KAD_RPC_MSG_FIELD_NAME_MAX_LEN 6
static const lookup_entry kad_rpc_msg_field_names[] = {
    { KAD_RPC_MSG_FIELD_TX_ID,    "t" },
    { KAD_RPC_MSG_FIELD_NODE_ID,  "id" },
    { KAD_RPC_MSG_FIELD_TYPE,     "y" },
    { KAD_RPC_MSG_FIELD_ERR,      "e" },
    { KAD_RPC_MSG_FIELD_METH,     "q" },
    { KAD_RPC_MSG_FIELD_TARGET,   "target" },
    { KAD_RPC_MSG_FIELD_NODES_ID, "nodes" },
    { 0,                          NULL },
};

typedef enum {
    KAD_GUID_T,
    KAD_RPC_MSG_TX_ID_T
} id_type;

bool id_copy(id_type t, void *id, const struct benc_literal *val)
{
    if (val->t != BENC_LITERAL_TYPE_STR) {
        log_error("Message node id not a string.");
        return false;
    }

    switch(t) {
    case KAD_GUID_T:
        if (val->s.len != KAD_GUID_SPACE_IN_BYTES) {
            log_error("Message node id has wrong length (%zu).", val->s.len);
            return false;
        }
        kad_guid_set(id, (unsigned char*)val->s.p);
        break;
    case KAD_RPC_MSG_TX_ID_T:
        if (val->s.len != KAD_RPC_MSG_TX_ID_LEN) {
            log_error("Message tx id has wrong length (%zu).", val->s.len);
            return false;
        }
        kad_rpc_msg_tx_id_set(id, (unsigned char*)val->s.p);
        break;
    default:
        log_error("Unknown id type provided.");
        return false;
    }

    return true;
}

/**
 * Populates @msg with @emit'ed @val'ue.
 *
 * "t" transaction id: 2 chars.
 * "y" message type: "q" for query, "r" for response, or "e" for error.
 *
 * "q" query method name: str.
 * "a" named arguments: dict.
 * "r" named return values: dict.
 *
 * "e" list: error code (int), error msg (str).
 */
bool benc_fill_rpc_msg()
{
    return true;
}

bool benc_decode_rpc_msg(struct kad_rpc_msg *msg, const char buf[], const size_t slen) {
    /* return benc_parse(msg, (benc_fill_fn)benc_fill_rpc_msg, buf, slen); */
    (void)msg;
    (void)buf;
    (void)slen;
    return true;
}

/**
 * Straight-forward serialization. NO VALIDATION is performed.
 */
bool benc_encode_rpc_msg(const struct kad_rpc_msg *msg, struct iobuf *buf)
{
    char tmps[2048];
    size_t tmps_len = 0;

    /* we avoid the burden of looking up into kad_rpc_msg_field_names just for single chars. */
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
                    kad_rpc_msg_field_names, KAD_RPC_MSG_FIELD_TARGET);
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
                    kad_rpc_msg_field_names, KAD_RPC_MSG_FIELD_NODES_ID);
                sprintf(tmps, "1:rd%zu:%sl", strlen(field_nodes), field_nodes);
                iobuf_append(buf, tmps, strlen(tmps));
                for (size_t i = 0; i < msg->nodes_len; i++) {
                    sprintf(tmps, "%d:", KAD_GUID_SPACE_IN_BYTES);
                    tmps_len = strlen(tmps);
                    memcpy(tmps + tmps_len, (char*)msg->nodes[i].id.bytes,
                           KAD_GUID_SPACE_IN_BYTES);
                    tmps_len += KAD_GUID_SPACE_IN_BYTES;
                    sprintf(tmps + tmps_len, "%zu:%s%zu:%s",
                            strlen(msg->nodes[i].host), msg->nodes[i].host,
                            strlen(msg->nodes[i].service), msg->nodes[i].service);
                    iobuf_append(buf, tmps, strlen(tmps)); // nodes
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
