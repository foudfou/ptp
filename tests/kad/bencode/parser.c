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


    struct benc_repr repr = {0};

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
    p = benc_node_find_key(&repr, &repr.n.buf[0], "d", 1);
    assert(p->typ == BENC_NODE_TYPE_DICT_ENTRY);
    assert(p->chd.len == 1);
    struct benc_node *child = benc_node_get_first_child(&repr, p);
    assert(child->typ == BENC_NODE_TYPE_LIST);
    assert(child->chd.len == 3);

    // list find where elt == dict with "v" key
    p = child;
    struct benc_node *d = NULL;
    for (size_t i = 0; i < p->chd.len; ++i) {
        const struct benc_node *n = benc_node_get_child(&repr, p, i);
        if (n->typ == BENC_NODE_TYPE_DICT) {
            d = benc_node_find_key(&repr, n, "v", 1);
            if (d) {
                break;
            }
        }
    }
    assert(d);
    assert(d->typ == BENC_NODE_TYPE_DICT_ENTRY);
    assert(d->chd.len == 1);
    child = benc_node_get_first_child(&repr, d);
    assert(child->typ == BENC_NODE_TYPE_LITERAL);
    struct benc_literal *node_lit = benc_node_get_literal(&repr, child);
    assert(node_lit->t == BENC_LITERAL_TYPE_STR);
    assert(node_lit->s.len == 4);
    assert(strncmp(node_lit->s.p, "none", node_lit->s.len) == 0);

    // int conversion ok
    p = benc_node_find_key(&repr, &repr.n.buf[0], "i", 1);
    assert(p->typ == BENC_NODE_TYPE_DICT_ENTRY);
    assert(p->chd.len == 1);
    child = benc_node_get_first_child(&repr, p);
    assert(child->typ == BENC_NODE_TYPE_LITERAL);
    // Literal nodes don't have children, so don't check chd.len
    /* assert(child->chd.len == 0); */
    node_lit = benc_node_get_literal(&repr, child);
    assert(node_lit->t == BENC_LITERAL_TYPE_INT);
    assert(node_lit->i == 42);


    strcpy(buf,"d");
    benc_repr_terminate(&repr);
    assert(!benc_parse(&repr, buf, strlen(buf)));

    strcpy(buf,"de");
    benc_repr_terminate(&repr);
    assert(benc_parse(&repr, buf, strlen(buf)));

    strcpy(buf, "dede");
    benc_repr_terminate(&repr);
    assert(!benc_parse(&repr, buf, strlen(buf)));

    strcpy(buf, "i5e");
    benc_repr_terminate(&repr);
    assert(benc_parse(&repr, buf, strlen(buf)));

    strcpy(buf, "i5e3:ddd");
    benc_repr_terminate(&repr);
    assert(!benc_parse(&repr, buf, strlen(buf)));

    // duplicate key entry
    strcpy(buf,"d2:abi12e2:abi34ee");
    benc_repr_terminate(&repr);
    assert(!benc_parse(&repr, buf, strlen(buf)));
    strcpy(buf,"d2:abi12e3:abci34ee"); // no dup
    benc_repr_terminate(&repr);
    assert(benc_parse(&repr, buf, strlen(buf)));

    // empty list and dictionary edge cases
    strcpy(buf, "le");
    benc_repr_terminate(&repr);
    assert(benc_parse(&repr, buf, strlen(buf)));
    assert(repr.n.buf[0].typ == BENC_NODE_TYPE_LIST);
    assert(repr.n.buf[0].chd.len == 0);

    // empty string
    strcpy(buf, "0:");
    benc_repr_terminate(&repr);
    assert(benc_parse(&repr, buf, strlen(buf)));
    node_lit = benc_node_get_literal(&repr, &repr.n.buf[0]);
    assert(node_lit->t == BENC_LITERAL_TYPE_STR);
    assert(node_lit->s.len == 0);

    // malformed inputs
    strcpy(buf, "");
    benc_repr_terminate(&repr);
    assert(!benc_parse(&repr, buf, strlen(buf)));

    strcpy(buf, "i42");
    benc_repr_terminate(&repr);
    assert(!benc_parse(&repr, buf, strlen(buf)));

    strcpy(buf, "lllli42eeeee"); // 4 levels of nesting
    benc_repr_terminate(&repr);
    assert(benc_parse(&repr, buf, strlen(buf)));

    strcpy(buf, "llllllli42eeeeeeee"); // 7 levels of nesting
    benc_repr_terminate(&repr);
    assert(benc_parse(&repr, buf, strlen(buf)));

    // nested dictionary structure
    strcpy(buf, "d1:ad1:bd1:ci42eeee"); // {a:{b:{c:42}}}
    benc_repr_terminate(&repr);
    assert(benc_parse(&repr, buf, strlen(buf)));

    // mixed structure (list containing dictionary)
    strcpy(buf, "ld1:ai1eee");
    benc_repr_terminate(&repr);
    assert(benc_parse(&repr, buf, strlen(buf)));

    // small dictionary
    strcpy(buf, "d1:ai1e1:bi2e1:ci3ee");
    benc_repr_terminate(&repr);
    assert(benc_parse(&repr, buf, strlen(buf)));

    // larger dictionary
    strcpy(buf, "d1:ai1e1:bi2e1:ci3e1:di4e1:ei5ee");
    benc_repr_terminate(&repr);
    assert(benc_parse(&repr, buf, strlen(buf)));

    // small list
    strcpy(buf, "li1ei2ei3ei4ei5ee");
    benc_repr_terminate(&repr);
    assert(benc_parse(&repr, buf, strlen(buf)));

    // larger list
    strcpy(buf, "li1ei2ei3ei4ei5ei6ei7ei8ee");
    benc_repr_terminate(&repr);
    assert(benc_parse(&repr, buf, strlen(buf)));

    // realistic bittorrent-like structure
    strcpy(buf, "d"
           "8:announce" "8:test.com"
           "7:comment" "12:test torrent"
           "13:creation date" "i1234567890e"
           "4:info"
           "d"
           "4:name" "8:test.txt"
           "12:piece length" "i32768e"
           "6:pieces" "20:aaaaaaaaaaaaaaaaaaaa"
           "6:length" "i1024e"
           "ee");
     benc_repr_terminate(&repr);
     assert(benc_parse(&repr, buf, strlen(buf)));

     d = benc_node_find_key(&repr, &repr.n.buf[0], "info", 4);
     assert(d && d->typ == BENC_NODE_TYPE_DICT_ENTRY);
     child = benc_node_get_first_child(&repr, d);
     assert(child);
     child = benc_node_find_key(&repr, child, "name", 4);
     assert(child);

     // integer overflow/underflow edge cases
     strcpy(buf, "i9223372036854775807e"); // LLONG_MAX
     benc_repr_terminate(&repr);
     assert(benc_parse(&repr, buf, strlen(buf)));
     node_lit = benc_node_get_literal(&repr, &repr.n.buf[0]);
     assert(node_lit->i == 9223372036854775807LL);

     strcpy(buf, "i-9223372036854775807e"); // LLONG_MIN + 1 (safer than LLONG_MIN)
     benc_repr_terminate(&repr);
     assert(benc_parse(&repr, buf, strlen(buf)));
     node_lit = benc_node_get_literal(&repr, &repr.n.buf[0]);
     assert(node_lit->i == -9223372036854775807LL);

     // string parsing with non-null binary data
     strcpy(buf, "4:\x01\x02\x03\x04");
     benc_repr_terminate(&repr);
     assert(benc_parse(&repr, buf, strlen(buf)));
     node_lit = benc_node_get_literal(&repr, &repr.n.buf[0]);
     assert(node_lit->t == BENC_LITERAL_TYPE_STR);
     assert(node_lit->s.len == 4);
     assert(node_lit->s.p[0] == 1 && node_lit->s.p[3] == 4);

     // string with null-byte
     memcpy(buf, "4:\x00\x01\x02\x03", 6);
     benc_repr_terminate(&repr);
     assert(benc_parse(&repr, buf, 6));
     node_lit = benc_node_get_literal(&repr, &repr.n.buf[0]);
     assert(node_lit->t == BENC_LITERAL_TYPE_STR);
     assert(node_lit->s.len == 4);
     assert(node_lit->s.p[0] == 0 && node_lit->s.p[3] == 3);

     // Unicode/binary string content
     memcpy(buf, "8:😋💚", 10);
     benc_repr_terminate(&repr);
     assert(benc_parse(&repr, buf, 10));
     node_lit = benc_node_get_literal(&repr, &repr.n.buf[0]);
     assert(node_lit->t == BENC_LITERAL_TYPE_STR);
     assert(node_lit->s.len == 8);

     // malformed input variations
     strcpy(buf, "i-e"); // invalid integer
     benc_repr_terminate(&repr);
     assert(!benc_parse(&repr, buf, strlen(buf)));

     strcpy(buf, "i12.5e"); // float in integer
     benc_repr_terminate(&repr);
     assert(!benc_parse(&repr, buf, strlen(buf)));

     strcpy(buf, "d1:ae"); // dictionary missing value
     benc_repr_terminate(&repr);
     assert(!benc_parse(&repr, buf, strlen(buf)));

     strcpy(buf, "di42e1:ae"); // dictionary with non-string key
     benc_repr_terminate(&repr);
     assert(!benc_parse(&repr, buf, strlen(buf)));

     strcpy(buf, "l"); // unterminated list
     benc_repr_terminate(&repr);
     assert(!benc_parse(&repr, buf, strlen(buf)));

     strcpy(buf, "d"); // unterminated dictionary
     benc_repr_terminate(&repr);
     assert(!benc_parse(&repr, buf, strlen(buf)));

     strcpy(buf, "6:short"); // string shorter than declared
     benc_repr_terminate(&repr);
     assert(!benc_parse(&repr, buf, strlen(buf)));
     strcpy(buf, "l6:shorti42ee");
     benc_repr_terminate(&repr);
     assert(!benc_parse(&repr, buf, strlen(buf)));

     strcpy(buf, "i42egarbage"); // trailing data
     benc_repr_terminate(&repr);
     assert(!benc_parse(&repr, buf, strlen(buf)));

     benc_repr_terminate(&repr);
     benc_parser_terminate(&parser);

     log_shutdown(LOG_TYPE_STDOUT);


     return 0;
}
