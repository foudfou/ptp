/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#ifndef BENCODE_PARSER_H
#define BENCODE_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "utils/growable.h"

#define BENC_PARSER_STACK_MAX   32
#define BENC_PARSER_STR_LEN_MAX 48 // our error messages may be >32

enum benc_literal_type {
    BENC_LITERAL_TYPE_NONE,
    BENC_LITERAL_TYPE_INT,
    BENC_LITERAL_TYPE_STR,
};

/* Literal values are stored into an array. */
struct benc_literal {
    enum benc_literal_type t;
    union {
        long long          i;
        /* TODO use an iobuf instead ? */
        struct {
            size_t         len;
            char           p[BENC_PARSER_STR_LEN_MAX];
        } s;
    };
};

enum benc_node_type {
    BENC_NODE_TYPE_NONE,
    BENC_NODE_TYPE_LITERAL,     /* 1 */
    BENC_NODE_TYPE_LIST,        /* 2 */
    BENC_NODE_TYPE_DICT,        /* 3 */
    BENC_NODE_TYPE_DICT_ENTRY,  /* 4 */
};

// Indices to underlying benc_node array. Since this underlying array is
// dynamic (realloc), we can't store pointers to them directly.
GROWABLE_GENERATE(index_lst, size_t, 8, 2)
GROWABLE_GENERATE_APPEND(index_lst, size_t)

#define INVALID_INDEX SIZE_MAX

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
struct benc_node {
    enum benc_node_type  typ;
    /* could be 'attr' or a list of, but actually only used for
       BENC_NODE_TYPE_DICT_ENTRY */
    char                 k[BENC_PARSER_STR_LEN_MAX];
    size_t               k_len;
    union {
        size_t           lit;  // index into repr.lit.buf
        struct index_lst chd;  // indices into repr.n.buf
    };
};

enum benc_tok {
    BENC_TOK_NONE,
    BENC_TOK_LITERAL,
    BENC_TOK_LIST,
    BENC_TOK_DICT,
    BENC_TOK_END,
};

// Real kad mesage vary in size: ping ~7 nodes,find_node query ~11 nodes,
// find_node response ~21 nodes, bootstrap responses potentially larger.
GROWABLE_GENERATE(benc_node_lst, struct benc_node, 16, 2)
GROWABLE_GENERATE_APPEND(benc_node_lst, struct benc_node)

GROWABLE_GENERATE(benc_literal_lst, struct benc_literal, 8, 2)
GROWABLE_GENERATE_APPEND(benc_literal_lst, struct benc_literal)

// Make sure to FREE-AFTER-USE with benc_repr_temrinate()!
struct benc_repr {
    struct benc_literal_lst lit;
    struct benc_node_lst    n;
};


static inline void benc_repr_terminate(struct benc_repr *repr) {
    benc_literal_lst_reset(&repr->lit);

    for (size_t i = 0; i < repr->n.len; ++i) {
        struct benc_node *n = &repr->n.buf[i];
        if ((n->typ == BENC_NODE_TYPE_LIST ||
             n->typ == BENC_NODE_TYPE_DICT ||
             n->typ == BENC_NODE_TYPE_DICT_ENTRY) &&
            n->chd.len > 0)
            index_lst_reset(&n->chd);
    }
    benc_node_lst_reset(&repr->n);
}

struct benc_parser {
    const char       *beg;      /* pointer to begin of buffer */
    const char       *cur;      /* pointer to current char in buffer */
    const char       *end;      /* pointer to end of buffer */
    bool              err;
    char              err_msg[BENC_PARSER_STR_LEN_MAX];
    size_t            stack[BENC_PARSER_STACK_MAX];  // indices into repr.n
    size_t            stack_off;
};

static inline struct benc_node *
benc_node_get_child(const struct benc_repr *repr, const struct benc_node *parent, size_t child_idx)
{
    if (child_idx >= parent->chd.len)
        return NULL;
    size_t node_idx = parent->chd.buf[child_idx];
    return &repr->n.buf[node_idx];
}

static inline struct benc_node *
benc_node_get_first_child(const struct benc_repr *repr, const struct benc_node *parent)
{
    return benc_node_get_child(repr, parent, 0);
}

static inline struct benc_literal *
benc_node_get_literal(const struct benc_repr *repr, const struct benc_node *node)
{
    if (node->typ != BENC_NODE_TYPE_LITERAL || node->lit >= repr->lit.len) {
        return NULL;
    }
    return &repr->lit.buf[node->lit];
}

struct benc_node *
benc_node_find_key(const struct benc_repr *repr,
                   const struct benc_node *dict,
                   const char              key[],
                   const size_t            key_len);

/**
 * Creates a tree-like representation of a bencode object from @buf.
 *
 * @param repr will hold the resulting bencode object. Use BENC_REPR_DECL_INIT
 *             to declare and initialize.
 */
bool benc_parse(struct benc_repr *repr, const char buf[], const size_t slen);

#endif /* BENCODE_PARSER_H */
