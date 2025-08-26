/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#ifndef BENCODE_PARSER_H
#define BENCODE_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "kad_defs.h"
#include "utils/growable.h"

#define BENC_PARSER_STACK_MAX   32
#define BENC_PARSER_STR_LEN_MAX 48 // our error messages may be >32

#define BENC_ROUTES_LITERAL_MAX KAD_GUID_SPACE_IN_BITS * KAD_K_CONST + 10

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
    enum benc_node_type       typ;
    /* could be 'attr' or a list of, but actually only used for
       BENC_NODE_TYPE_DICT_ENTRY */
    char                      k[BENC_PARSER_STR_LEN_MAX];
    size_t                    k_len;
    union {
        struct benc_literal  *lit;
        struct index_lst      chd;
    };
};

enum benc_tok {
    BENC_TOK_NONE,
    BENC_TOK_LITERAL,
    BENC_TOK_LIST,
    BENC_TOK_DICT,
    BENC_TOK_END,
};

GROWABLE_GENERATE(benc_node_lst, struct benc_node, 8, 2)
GROWABLE_GENERATE_APPEND(benc_node_lst, struct benc_node)

struct benc_repr {
    struct benc_literal *lit;
    size_t               lit_len;
    size_t               lit_off;
    struct benc_node_lst n;
};

// Global representations for parser. Static as quite large and might blow the
// stack.
extern struct benc_literal repr_literals[];
extern struct benc_repr repr;

static inline void benc_repr_init() {
    memset(repr_literals, 0, sizeof(struct benc_literal) * BENC_ROUTES_LITERAL_MAX);
    repr.lit = repr_literals;
    repr.lit_len = (BENC_ROUTES_LITERAL_MAX);
    repr.lit_off = 0;
    memset(&repr.n, 0, sizeof(struct index_lst));
}

static inline void benc_repr_terminate() {
    repr.lit_off = 0;

    for (size_t i = 0; i < repr.n.len; ++i) {
        struct benc_node *n = &repr.n.buf[i];
        if ((n->typ == BENC_NODE_TYPE_LIST ||
             n->typ == BENC_NODE_TYPE_DICT ||
             n->typ == BENC_NODE_TYPE_DICT_ENTRY) &&
            n->chd.len > 0)
            index_lst_reset(&n->chd);
    }
    benc_node_lst_reset(&repr.n);
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
benc_node_get_child(const struct benc_node *parent, size_t child_idx)
{
    if (child_idx >= parent->chd.len)
        return NULL;
    size_t node_idx = parent->chd.buf[child_idx];
    return &repr.n.buf[node_idx];
}

static inline struct benc_node *
benc_node_get_first_child(const struct benc_node *parent)
{
    return benc_node_get_child(parent, 0);
}

struct benc_node *
benc_node_find_key(const struct benc_node *dict,
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
