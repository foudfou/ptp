/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#ifndef BENCODE_PARSER_H
#define BENCODE_PARSER_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "kad_defs.h"

#define BENC_PARSER_STACK_MAX   32

enum benc_parser_type {
    BENC_PARSER_GLOBAL,    /* Use global static buffers */
    BENC_PARSER_NET,       /* Small stack-allocated for network messages */
};

#define BENC_ROUTES_LITERAL_MAX KAD_GUID_SPACE_IN_BITS * KAD_K_CONST + 10
#define BENC_ROUTES_NODES_MAX   KAD_GUID_SPACE_IN_BITS * KAD_K_CONST + 32

#define BENC_NODE_CHILDREN_MAX  256
#define BENC_PARSER_STR_LEN_MAX 256

/* Parser type constants - conservative limits for network messages */
#define BENC_NET_CHILDREN_MAX 8     /* Max children per dict/list node */
#define BENC_NET_STR_MAX      32    /* Node IDs are 20 bytes, method names ~10 */
#define BENC_NET_NODES_MAX    16    /* find_node should have ~(8 + tx_id + msg_type) */
#define BENC_NET_LITERALS_MAX 16    /* String/int literals, round number for alignment */

enum benc_literal_type {
    BENC_LITERAL_TYPE_NONE,
    BENC_LITERAL_TYPE_INT,
    BENC_LITERAL_TYPE_STR,
};

/* Literal values are stored into an array. */
#define BENC_LITERAL_GEN(suf, str_max)         \
    struct benc_literal##suf {                      \
        enum benc_literal_type t;                   \
        union {                                     \
            long long      i;                       \
            struct {                                \
                /* TODO use an iobuf instead ? */   \
                size_t     len;                     \
                char       p[str_max];              \
            } s;                                    \
        };                                          \
    };

BENC_LITERAL_GEN(, BENC_PARSER_STR_LEN_MAX)
BENC_LITERAL_GEN(_net, BENC_NET_STR_MAX)

enum benc_node_type {
    BENC_NODE_TYPE_NONE,
    BENC_NODE_TYPE_LITERAL,     /* 1 */
    BENC_NODE_TYPE_LIST,        /* 2 */
    BENC_NODE_TYPE_DICT,        /* 3 */
    BENC_NODE_TYPE_DICT_ENTRY,  /* 4 */
};

/* Parsing consists in building a representation of the bencode object. This is
   done via 2 data structures: a tree of nodes and a list of literal values for
   str|int. Nodes can be of type: dict|dict_entry|list|literal. In practice
   nodes are stored into an array. Each node points to other nodes.

  {d:["a", 1, {v:"none"}], i:42} translates to

  dict
  ├──entry, key=d
  │  └──list
  │     ├──str=literals[0]
  │     ├──int=literals[1]
  │     └──dict
  │        └──entry, key=v
  │           └──str=literals[2]
  └──entry, key=i
     └──str=literals[3]

     literals=["a", 1, "none", 42]
*/
#define BENC_NODE_GEN(suf, str_max, chd_max)                        \
    struct benc_node##suf {                                         \
        enum benc_node_type      typ;                               \
        /* could be 'attr' or a list of, but actually only used for
           BENC_NODE_TYPE_DICT_ENTRY */                             \
        char                     k[str_max];                        \
        size_t                   k_len;                             \
        union {                                                     \
            struct benc_literal *lit;                               \
            /* TODO consider using a list */                        \
            struct benc_node    *chd[chd_max];                      \
        };                                                          \
        size_t                   chd_off;                           \
    };

BENC_NODE_GEN(, BENC_PARSER_STR_LEN_MAX, BENC_NODE_CHILDREN_MAX)
BENC_NODE_GEN(_net, BENC_NET_STR_MAX, BENC_NET_CHILDREN_MAX)

enum benc_tok {
    BENC_TOK_NONE,
    BENC_TOK_LITERAL,
    BENC_TOK_LIST,
    BENC_TOK_DICT,
    BENC_TOK_END,
};

struct benc_repr {
    struct benc_literal *lit;
    size_t               lit_len;
    size_t               lit_off;
    struct benc_node    *n;
    size_t               n_len;
    size_t               n_off;
};

/* Network buffers - separate from representation for flexibility */
struct benc_net_bufs {
    struct benc_literal_net literals[BENC_NET_LITERALS_MAX];
    struct benc_node_net    nodes[BENC_NET_NODES_MAX];
};

// Global representations for parser. Static as quite large and might blow the
// stack.
extern struct benc_literal repr_literals[];
extern struct benc_node repr_nodes[];
extern struct benc_repr repr;

static inline void benc_repr_init() {
    memset(repr_literals, 0, sizeof(struct benc_literal) * BENC_ROUTES_LITERAL_MAX);
    memset(repr_nodes, 0, sizeof(struct benc_node) * BENC_ROUTES_NODES_MAX);
    repr.lit = repr_literals;
    repr.lit_len = (BENC_ROUTES_LITERAL_MAX);
    repr.lit_off = 0;
    repr.n = repr_nodes;
    repr.n_len = (BENC_ROUTES_NODES_MAX);
    repr.n_off = 0;
}

static inline void benc_repr_net_init(struct benc_repr *repr, struct benc_net_bufs *bufs) {
    memset(bufs, 0, sizeof(*bufs));
    repr->lit = (struct benc_literal*)bufs->literals;
    repr->lit_len = BENC_NET_LITERALS_MAX;
    repr->lit_off = 0;
    repr->n = (struct benc_node*)bufs->nodes;
    repr->n_len = BENC_NET_NODES_MAX;
    repr->n_off = 0;
}

struct benc_parser {
    const char       *beg;      /* pointer to begin of buffer */
    const char       *cur;      /* pointer to current char in buffer */
    const char       *end;      /* pointer to end of buffer */
    bool              err;
    char              err_msg[BENC_PARSER_STR_LEN_MAX];
    struct benc_node *stack[BENC_PARSER_STACK_MAX];
    size_t            stack_off;
};

struct benc_node* benc_node_find_key(const struct benc_node *dict,
                                     const char key[], const size_t key_len);

/**
 * Creates a tree-like representation of a bencode object from @buf.
 *
 * @param repr will hold the resulting bencode object. Use BENC_REPR_DECL_INIT
 *             to declare and initialize.
 */
bool benc_parse(struct benc_repr *repr, const char buf[], const size_t slen);

bool benc_parse_typed(enum benc_parser_type type, const char buf[], const size_t slen,
                      struct benc_repr *result_repr, struct benc_net_bufs *bufs);

#endif /* BENCODE_PARSER_H */
