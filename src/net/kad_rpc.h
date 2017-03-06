/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef KAD_RPC_H
#define KAD_RPC_H

/**
 * KRPC Protocol as defined in http://www.bittorrent.org/beps/bep_0005.html.
 */
#include <netdb.h>
#include <stdbool.h>
#include <stdlib.h>
#include "iobuf.h"
#include "kad.h"

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

struct krpc_node_info {
    kad_guid id;
    char     host[NI_MAXHOST];
    char     service[NI_MAXSERV];
};

/**
 * Naive flattened dictionary for all possible messages.
 */
struct kad_rpc_msg {
    char                  tx_id[2]; /* t */
    kad_guid              node_id;  /* from {a,r} dict: id_str */
    enum kad_rpc_type     type;     /* y {q,r,e} */
    unsigned long long    err_code;
    struct iobuf          err_msg;
    enum kad_rpc_type     meth;     /* q {"ping","find_node"} */
    kad_guid              target;   /* from {a,r} dict: target, nodes */
    struct krpc_node_info nodes[KAD_K_CONST]; /* from {a,r} dict: target, nodes */
};

enum kad_rpc_stack {
    KAD_RPC_STACK_NONE,
    KAD_RPC_STACK_VAL,          /* 1 */
    KAD_RPC_STACK_INT,
    KAD_RPC_STACK_STR,
    KAD_RPC_STACK_LIST,
    KAD_RPC_STACK_DICT_KEY,     /* 5 */
    KAD_RPC_STACK_DICT_VAL,     /* 6 */
    KAD_RPC_STACK_END,
};

#define KAD_RPC_PARSER_STACK_MAX    32
#define KAD_RPC_PARSER_ERR_MSG_MAX 256
#define KAD_RPC_PARSER_STR_MAX     256

struct kad_rpc_parser {
    const char         *beg;    /* pointer to begin of buffer */
    const char         *cur;    /* pointer to current char in buffer */
    const char         *end;    /* pointer to end of buffer */
    bool                err;
    char                err_msg[KAD_RPC_PARSER_ERR_MSG_MAX];
    enum kad_rpc_stack  stack[KAD_RPC_PARSER_STACK_MAX];
    size_t              stack_off;
    struct kad_rpc_msg  msg;
};


bool kad_rpc_parse(const char host[], const char service[],
                   const char buf[], const size_t slen);


#endif /* KAD_RPC_H */
