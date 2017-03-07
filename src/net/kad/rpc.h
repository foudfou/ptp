/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef KAD_RPC_H
#define KAD_RPC_H

/**
 * KRPC Protocol as defined in http://www.bittorrent.org/beps/bep_0005.html.
 */

#include <stdbool.h>
#include "net/kad/dht.h"
#include "utils/misc.h"

#define KAD_RPC_STR_MAX 256

enum kad_rpc_type {
    KAD_RPC_TYPE_NONE,
    KAD_RPC_TYPE_ERROR,
    KAD_RPC_TYPE_QUERY,
    KAD_RPC_TYPE_RESPONSE,
};

enum kad_rpc_meth {
    KAD_RPC_METH_NONE,
    KAD_RPC_METH_PING,
    KAD_RPC_METH_FIND_NODE,
};

struct kad_rpc_node_info {
    kad_guid id;
    char     host[NI_MAXHOST];
    char     service[NI_MAXSERV];
};

/**
 * Naive flattened dictionary for all possible messages.
 *
 * The protocol being relatively tiny, data size considered limited (a Kad
 * message must fit into an UDP buffer: no application flow control), every
 * awaited value should fit into well-defined fields. So we don't bother to
 * serialize lists and dicts for now.
 */
struct kad_rpc_msg {
    char                     tx_id[2]; /* t */
    kad_guid                 node_id; /* from {a,r} dict: id_str */
    enum kad_rpc_type        type; /* y {q,r,e} */
    unsigned long long       err_code;
    char                     err_msg[KAD_RPC_STR_MAX];
    enum kad_rpc_type        meth; /* q {"ping","find_node"} */
    kad_guid                 target; /* from {a,r} dict: target, nodes */
    struct kad_rpc_node_info nodes[KAD_K_CONST]; /* from {a,r} dict: target, nodes */
};

enum kad_rpc_msg_field {
    KAD_RPC_MSG_FIELD_NONE,
    KAD_RPC_MSG_FIELD_TX_ID,
    KAD_RPC_MSG_FIELD_NODE_ID,
    KAD_RPC_MSG_FIELD_TYPE,
    KAD_RPC_MSG_FIELD_ERR_CODE,
    KAD_RPC_MSG_FIELD_ERR_MSG,
    KAD_RPC_MSG_FIELD_METH,
    KAD_RPC_MSG_FIELD_TARGET,
    KAD_RPC_MSG_FIELD_NODES,
};

static const lookup_entry kad_rpc_msg_field_names[] = {
    { KAD_RPC_MSG_FIELD_TX_ID,    "t" },
    { KAD_RPC_MSG_FIELD_NODE_ID,  "id" },
    { KAD_RPC_MSG_FIELD_TYPE,     "y" },
    { KAD_RPC_MSG_FIELD_ERR_CODE, "" },
    { KAD_RPC_MSG_FIELD_ERR_MSG,  "" },
    { KAD_RPC_MSG_FIELD_METH,     "q" },
    { KAD_RPC_MSG_FIELD_TARGET,   "target" },
    { KAD_RPC_MSG_FIELD_NODES,    "nodes" },
    { 0,                          NULL },
};

bool kad_rpc_parse(const char host[], const char service[],
                   const char buf[], const size_t slen);

#endif /* KAD_RPC_H */
