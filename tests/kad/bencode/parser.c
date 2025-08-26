/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include <stdio.h>
#include "net/kad/bencode/parser.c"        // testing static functions

#define BENC_PARSER_BUF_MAX 1400


int main ()
{
    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_CRIT)));

    char buf[BENC_PARSER_BUF_MAX] = "i-300e";
    struct benc_parser parser = {0};
    benc_parser_init(&parser, buf, strlen(buf));

    struct benc_literal lit;
    assert(benc_extract_int(&parser, &lit));
    assert(lit.t == BENC_LITERAL_TYPE_INT);
    assert(lit.i == -300);

    strcpy(buf, "i3.14e");
    benc_parser_init(&parser, buf, strlen(buf));
    assert(!benc_extract_int(&parser, &lit));
    assert(parser.err);

    strcpy(buf, "i9223372036854775808e"); // overflow
    benc_parser_init(&parser, buf, strlen(buf));
    assert(!benc_extract_int(&parser, &lit));
    assert(parser.err);

    strcpy(buf, "4:spam");
    benc_parser_init(&parser, buf, strlen(buf));
    assert(benc_extract_str(&parser, &lit));
    assert(lit.s.len == 4);
    assert(strncmp(lit.s.p, "spam", lit.s.len) == 0);
    assert(*parser.cur == '\0');
    assert(POINTER_OFFSET(parser.beg, parser.cur) == 6);

    strcpy(buf, "2:\x11\x22");
    benc_parser_init(&parser, buf, strlen(buf));
    assert(benc_extract_str(&parser, &lit));
    assert(lit.s.len == 2);
    assert(strncmp(lit.s.p, "\x11\x22", lit.s.len) == 0);
    assert(POINTER_OFFSET(parser.beg, parser.cur) == 4);

    strcpy(buf, "65535:anything");  // overflow
    benc_parser_init(&parser, buf, strlen(buf));
    assert(!benc_extract_str(&parser, &lit));
    assert(parser.err);


    benc_repr_init();

    // {d:["a",1,{v:"none"}],i:42}
    strcpy(buf,"d1:dl1:ai1ed1:v4:noneee1:ii42ee");
    assert(benc_parse(&repr, buf, strlen(buf)));
    assert(repr.lit.len == 4);
    assert(repr.n.len == 10);
    assert(repr.lit.buf[0].t == BENC_LITERAL_TYPE_STR);
    assert(repr.lit.buf[0].s.len == 1);
    assert(repr.lit.buf[0].s.p[0] == 'a');

    // navigating
    assert(repr.n.buf[0].typ == BENC_NODE_TYPE_DICT);
    struct benc_node *p = NULL;
    p = benc_node_find_key(&repr.n.buf[0], "d", 1);
    assert(p->typ == BENC_NODE_TYPE_DICT_ENTRY);
    assert(p->chd.len == 1);
    struct benc_node *child = benc_node_get_first_child(p);
    assert(child->typ == BENC_NODE_TYPE_LIST);
    assert(child->chd.len == 3);

    // list find where elt == dict with "v" key
    p = child;
    struct benc_node *d = NULL;
    for (size_t i = 0; i < p->chd.len; ++i) {
        const struct benc_node *n = benc_node_get_child(p, i);
        if (n->typ == BENC_NODE_TYPE_DICT) {
            d = benc_node_find_key(n, "v", 1);
            if (d) {
                break;
            }
        }
    }
    assert(d);
    assert(d->typ == BENC_NODE_TYPE_DICT_ENTRY);
    assert(d->chd.len == 1);
    child = benc_node_get_first_child(d);
    assert(child->typ == BENC_NODE_TYPE_LITERAL);
    struct benc_literal *node_lit = benc_node_get_literal(child);
    assert(node_lit->t == BENC_LITERAL_TYPE_STR);
    assert(node_lit->s.len == 4);
    assert(strncmp(node_lit->s.p, "none", node_lit->s.len) == 0);

    // int conversion ok
    p = benc_node_find_key(&repr.n.buf[0], "i", 1);
    assert(p->typ == BENC_NODE_TYPE_DICT_ENTRY);
    assert(p->chd.len == 1);
    child = benc_node_get_first_child(p);
    assert(child->typ == BENC_NODE_TYPE_LITERAL);
    // Literal nodes don't have children, so don't check chd.len
    /* assert(child->chd.len == 0); */
    node_lit = benc_node_get_literal(child);
    assert(node_lit->t == BENC_LITERAL_TYPE_INT);
    assert(node_lit->i == 42);


    strcpy(buf,"d");
    benc_repr_terminate(); benc_repr_init();
    assert(!benc_parse(&repr, buf, strlen(buf)));

    strcpy(buf,"de");
    benc_repr_terminate(); benc_repr_init();
    assert(benc_parse(&repr, buf, strlen(buf)));

    strcpy(buf, "dede");
    benc_repr_terminate(); benc_repr_init();
    assert(!benc_parse(&repr, buf, strlen(buf)));

    strcpy(buf, "i5e");
    benc_repr_terminate(); benc_repr_init();
    assert(benc_parse(&repr, buf, strlen(buf)));

    strcpy(buf, "i5e3:ddd");
    benc_repr_terminate(); benc_repr_init();
    assert(!benc_parse(&repr, buf, strlen(buf)));

    // duplicate key entry
    strcpy(buf,"d2:abi12e2:abi34ee");
    benc_repr_terminate(); benc_repr_init();
    assert(!benc_parse(&repr, buf, strlen(buf)));
    strcpy(buf,"d2:abi12e3:abci34ee");
    benc_repr_terminate(); benc_repr_init();
    assert(benc_parse(&repr, buf, strlen(buf)));

    // FIXME to be continued...

    benc_repr_terminate();
    benc_parser_terminate(&parser);

    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
