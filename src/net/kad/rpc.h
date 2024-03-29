/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef KAD_RPC_H
#define KAD_RPC_H

/**
 * KRPC Protocol as defined in http://www.bittorrent.org/beps/bep_0005.html.
 */
#include <limits.h>
#include <stdbool.h>
#include "net/iobuf.h"
#include "net/kad/lookup.h"
#include "net/kad/routes.h"
#include "utils/list.h"
#include "utils/lookup.h"
#include "net/kad/bencode/parser.h"

// #include "net/kad/req_lru.h"
struct req_lru;


// TODO tune and move to defs
#define KAD_RPC_QUERY_TIMEOUT_MILLIS 60000

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

enum kad_rpc_err {
    KAD_RPC_ERR_NONE,
    KAD_RPC_ERR_GENERIC      = 201,
    KAD_RPC_ERR_SERVER       = 202,
    KAD_RPC_ERR_PROTOCOL     = 203, // « such as a malformed packet, invalid
                                    // arguments, or bad token »
    KAD_RPC_ERR_METH_UNKNOWN = 204,
};

static const lookup_entry kad_rpc_err_names[] = {
    { KAD_RPC_ERR_GENERIC,      "Generic Error" },
    { KAD_RPC_ERR_SERVER,       "Server Error" },
    { KAD_RPC_ERR_PROTOCOL,     "Protocol Error" },
    { KAD_RPC_ERR_METH_UNKNOWN, "Method Unknown" },
    { 0,                        NULL },
};

/**
 * KRPC message.
 *
 * Initialize KAD_RPC_MSG_INIT().
 */
struct kad_rpc_msg {
    kad_rpc_msg_tx_id    tx_id;   // t
    kad_guid             node_id; // from {a,r} dict: id_str
    enum kad_rpc_type    type;    // y {q,r,e}
    enum kad_rpc_meth    meth;    // q {"ping","find_node"}
    unsigned long long   err_code;
    char                 err_msg[BENC_PARSER_STR_LEN_MAX];
    kad_guid             target;             // from {a,r} dict: target, nodes
    struct kad_node_info nodes[KAD_K_CONST]; // from {a,r} dict: target, nodes
    size_t               nodes_len;
};

struct kad_rpc_query {
    struct list_item     litem;
    struct list_item     hitem;
    long long            created; // for expiring queries
    struct kad_rpc_msg   msg;
    struct kad_node_info node;
};

struct kad_ctx {
    struct kad_routes *routes;
    struct req_lru    *reqs_out;
    struct kad_lookup  lookup;
    struct list_item  *timers;
    int                sock;
};

int kad_rpc_init(struct kad_ctx *ctx, const char conf_dir[]);
void kad_rpc_terminate(struct kad_ctx *ctx, const char conf_dir[]);

bool kad_rpc_handle(struct kad_ctx *ctx, const struct sockaddr_storage *addr,
                    const char buf[], const size_t slen, struct iobuf *rsp);
void kad_rpc_msg_log(const struct kad_rpc_msg *msg);

bool kad_rpc_query_create(struct iobuf *buf, struct kad_rpc_query *query, const struct kad_ctx *ctx);


#endif /* KAD_RPC_H */
