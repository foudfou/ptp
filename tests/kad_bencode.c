/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "net/kad/bencode.c"        // testing static functions
#include "net/kad/rpc.h"
#include "kad_tests.h"

#define BENC_PARSER_BUF_MAX 1400

#include <limits.h>

int main ()
{
    char buf[BENC_PARSER_BUF_MAX] = "i-300e";
    size_t slen = strlen(buf);
    struct benc_parser parser;
    benc_parser_init(&parser, buf, slen);

    struct benc_val val;
    assert(benc_parse_int(&parser, &val));
    assert(val.i == -300);

    strcpy(buf, "i3.14e");
    slen = strlen(buf);
    benc_parser_init(&parser, buf, slen);
    assert(!benc_parse_int(&parser, &val));
    assert(parser.err);

    strcpy(buf, "i9223372036854775808e"); // overflow
    slen = strlen(buf);
    benc_parser_init(&parser, buf, slen);
    assert(!benc_parse_int(&parser, &val));
    assert(parser.err);

    strcpy(buf, "4:spam");
    slen = strlen(buf);
    benc_parser_init(&parser, buf, slen);
    assert(benc_parse_str(&parser, &val));
    assert(val.s.len == 4);
    assert(strncmp(val.s.p, "spam", val.s.len) == 0);
    assert(*parser.cur == '\0');
    assert(POINTER_OFFSET(parser.beg, parser.cur) == 6);

    strcpy(buf, "2:\x11\x22");
    slen = strlen(buf);
    benc_parser_init(&parser, buf, slen);
    assert(benc_parse_str(&parser, &val));
    assert(val.s.len == 2);
    assert(strncmp(val.s.p, "\x11\x22", val.s.len) == 0);
    assert(POINTER_OFFSET(parser.beg, parser.cur) == 4);

    strcpy(buf, "65535:anything");  // overflow
    slen = strlen(buf);
    benc_parser_init(&parser, buf, slen);
    assert(!benc_parse_str(&parser, &val));
    assert(parser.err);

    benc_parser_terminate(&parser);


    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_CRIT)));

    struct kad_rpc_msg msg = {0};

    // fail: incomplete
    strcpy(buf, "d");
    slen = strlen(buf);
    assert(!benc_decode(&msg, buf, slen));

    // ok: void dict
    strcpy(buf, "de");
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode(&msg, buf, slen));

    /* // fail: syntax */
    /* strcpy(buf, "dede"); */
    /* slen = strlen(buf); */
    /* memset(&msg, 0, sizeof(msg)); */
    /* assert(!benc_decode(&msg, buf, slen)); */

    strcpy(buf, KAD_TEST_ERROR);
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode(&msg, buf, slen));

    strcpy(buf, KAD_TEST_PING_QUERY);
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode(&msg, buf, slen));

    strcpy(buf, KAD_TEST_PING_RESPONSE);
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode(&msg, buf, slen));

    strcpy(buf, KAD_TEST_FIND_NODE_QUERY);
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode(&msg, buf, slen));

    strcpy(buf, KAD_TEST_FIND_NODE_RESPONSE);
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode(&msg, buf, slen));

    strcpy(buf, KAD_TEST_FIND_NODE_RESPONSE_BOGUS);
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(!benc_decode(&msg, buf, slen));

    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
