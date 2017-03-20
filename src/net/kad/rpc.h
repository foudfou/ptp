/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef KAD_RPC_H
#define KAD_RPC_H

/**
 * KRPC Protocol as defined in http://www.bittorrent.org/beps/bep_0005.html.
 */

#include <stdbool.h>
#include "net/kad/dht.h"
#include "utils/list.h"
#include "utils/lookup.h"

#define KAD_RPC_STR_MAX       256
#define KAD_RPC_MSG_TX_ID_LEN 2

enum kad_rpc_type {
    KAD_RPC_TYPE_NONE,
    KAD_RPC_TYPE_ERROR,
    KAD_RPC_TYPE_QUERY,
    KAD_RPC_TYPE_RESPONSE,
};

static const lookup_entry kad_rpc_type_names[] = {
    { KAD_RPC_TYPE_ERROR,    "e" },
    { KAD_RPC_TYPE_QUERY,    "q" },
    { KAD_RPC_TYPE_RESPONSE, "r" },
    { 0,                     NULL },
};

enum kad_rpc_meth {
    KAD_RPC_METH_NONE,
    KAD_RPC_METH_PING,
    KAD_RPC_METH_FIND_NODE,
};

static const lookup_entry kad_rpc_meth_names[] = {
    { KAD_RPC_METH_PING,      "ping" },
    { KAD_RPC_METH_FIND_NODE, "find_node" },
    { 0,                      NULL },
};

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

/**
 * Naive flattened dictionary for all possible messages.
 *
 * The protocol being relatively tiny, data size considered limited (a Kad
 * message must fit into an UDP buffer: no application flow control), every
 * awaited value should fit into well-defined fields. So we don't bother to
 * serialize lists and dicts for now.
 *
 * Although we have all mechanics to build a bstree with dict_key as the key,
 * and struct benc_val as data, it seems more convenient to avoid malloc(3) at
 * this stage.
 */
struct kad_rpc_msg {
    struct list_item     item;
    unsigned char        tx_id[KAD_RPC_MSG_TX_ID_LEN]; /* t */
    kad_guid             node_id; /* from {a,r} dict: id_str */
    enum kad_rpc_type    type;  /* y {q,r,e} */
    unsigned long long   err_code;
    char                 err_msg[KAD_RPC_STR_MAX];
    enum kad_rpc_meth    meth;  /* q {"ping","find_node"} */
    kad_guid             target; /* from {a,r} dict: target, nodes */
    /*
     * We diverge here from the BitTorrent spec where a compact node info is
     * node_id (20B) + IP (4B) + port (2B), assuming ip4. A node info comprises
     * 3 strings. Which implies that node infos are encoded in lists: ["id1",
     * "host1", "service1", "id2", "host2", "service3", ...].
     */
    struct kad_node_info nodes[KAD_K_CONST]; /* from {a,r} dict: target, nodes */
    size_t               nodes_len;
};

struct kad_rpc_node_pair {
    struct list_item     item;
    struct kad_node_info old;
    struct kad_node_info new;
};

struct kad_ctx {
    struct kad_dht   *dht;
    struct list_item  queries; // kad_rcp_msg list
    struct list_item  insertq; // kad_rpc_node_pair list
};

bool kad_rpc_init(struct kad_ctx *ctx);
void kad_rpc_terminate(struct kad_ctx *ctx);

bool kad_rpc_handle(struct kad_ctx *ctx, const char host[], const char service[],
                    const char buf[], const size_t slen);
void kad_rpc_msg_log(const struct kad_rpc_msg *msg);

#endif /* KAD_RPC_H */
