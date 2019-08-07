/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "net/kad/bencode/parser.c"        // testing static functions

#define BENC_PARSER_BUF_MAX 1400


int main ()
{
    char buf[BENC_PARSER_BUF_MAX] = "i-300e";
    struct benc_parser parser;
    benc_parser_init(&parser, buf, strlen(buf));

    struct benc_val val;
    assert(benc_parse_int(&parser, &val));
    assert(val.i == -300);

    strcpy(buf, "i3.14e");
    benc_parser_init(&parser, buf, strlen(buf));
    assert(!benc_parse_int(&parser, &val));
    assert(parser.err);

    strcpy(buf, "i9223372036854775808e"); // overflow
    benc_parser_init(&parser, buf, strlen(buf));
    assert(!benc_parse_int(&parser, &val));
    assert(parser.err);

    strcpy(buf, "4:spam");
    benc_parser_init(&parser, buf, strlen(buf));
    assert(benc_parse_str(&parser, &val));
    assert(val.s.len == 4);
    assert(strncmp(val.s.p, "spam", val.s.len) == 0);
    assert(*parser.cur == '\0');
    assert(POINTER_OFFSET(parser.beg, parser.cur) == 6);

    strcpy(buf, "2:\x11\x22");
    benc_parser_init(&parser, buf, strlen(buf));
    assert(benc_parse_str(&parser, &val));
    assert(val.s.len == 2);
    assert(strncmp(val.s.p, "\x11\x22", val.s.len) == 0);
    assert(POINTER_OFFSET(parser.beg, parser.cur) == 4);

    strcpy(buf, "65535:anything");  // overflow
    benc_parser_init(&parser, buf, strlen(buf));
    assert(!benc_parse_str(&parser, &val));
    assert(parser.err);

    benc_parser_terminate(&parser);


    return 0;
}
