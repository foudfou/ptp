/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#ifndef BENCODE_PARSER_H
#define BENCODE_PARSER_H

#include <stddef.h>
#include <stdbool.h>

#define BENC_PARSER_STACK_MAX      32
#define BENC_PARSER_STR_LEN_MAX    256
#define BENC_NODE_CHILDREN_MAX 256

enum benc_literal_type {
    BENC_LITERAL_TYPE_NONE,
    BENC_LITERAL_TYPE_INT,
    BENC_LITERAL_TYPE_STR,
};

/* Literal values are stored into an array. */
struct benc_literal {
    enum benc_literal_type t;
    union {
        long long      i;
        struct {
            /* TODO use an iobuf instead ? */
            size_t     len;
            char       p[BENC_PARSER_STR_LEN_MAX];
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

/* Parsing consists in building a representation of the bncode object. This is
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
    enum benc_node_type      typ;
     // could be 'attr' or a list of, but actually only used for
     // BENC_NODE_TYPE_DICT_ENTRY
    char                     k[BENC_PARSER_STR_LEN_MAX];
    size_t                   k_len;
    union {
        struct benc_literal *lit;
        // TODO consider using a list
        struct benc_node    *chd[BENC_NODE_CHILDREN_MAX];
    };
    size_t                   chd_off;
};

enum benc_tok {
    BENC_TOK_NONE,
    BENC_TOK_LITERAL,
    BENC_TOK_LIST,
    BENC_TOK_DICT,
    BENC_TOK_END,
};

#define BENC_REPR_DECL_INIT(name, max_literals, max_nodes)              \
    struct benc_literal name##_literals[max_literals] = {0};            \
    struct benc_node name##_nodes[max_nodes] = {0};                     \
    struct benc_repr name = {                                           \
        .lit=name##_literals, .lit_len=(max_literals), .lit_off=0,      \
        .n=name##_nodes,      .n_len=(max_nodes),      .n_off=0         \
    };

struct benc_repr {
    struct benc_literal *lit;
    size_t               lit_len;
    size_t               lit_off;
    struct benc_node    *n;
    size_t               n_len;
    size_t               n_off;
};

struct benc_parser {
    const char       *beg;      /* pointer to begin of buffer */
    const char       *cur;      /* pointer to current char in buffer */
    const char       *end;      /* pointer to end of buffer */
    bool              err;
    char              err_msg[BENC_PARSER_STR_LEN_MAX];
    struct benc_node *stack[BENC_PARSER_STACK_MAX];
    size_t            stack_off;
};

struct benc_node* benc_node_find_dict_entry(const struct benc_node *dict,
                                            const char key[], const size_t key_len);
/**
 * Creates a tree-like representation of a bencode object from @buf.
 *
 * @param repr will hold the resulting bencode object. Use BENC_REPR_DECL_INIT
 *             to declare and initialize.
 */
bool benc_parse(struct benc_repr *repr, const char buf[], const size_t slen);

#endif /* BENCODE_PARSER_H */
