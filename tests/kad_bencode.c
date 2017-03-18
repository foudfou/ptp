/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "net/kad/bencode.c"        // testing static functions
#include "net/kad/rpc.h"

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

    // error {"t":"aa", "y":"e", "e":[201, "A Generic Error Ocurred"]}
    strcpy(buf, "d1:eli201e23:A Generic Error Ocurrede1:t2:aa1:y1:ee");
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode(&msg, buf, slen));

    // ping query {"t":"aa", "y":"q", "q":"ping", "a":{"id":"abcdefghij0123456789"}}
    strcpy(buf, "d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t2:aa1:y1:qe");
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode(&msg, buf, slen));

    // ping response {"t":"aa", "y":"r", "r": {"id":"mnopqrstuvwxyz123456"}}
    strcpy(buf, "d1:rd2:id20:mnopqrstuvwxyz123456e1:t2:aa1:y1:re");
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode(&msg, buf, slen));

    // find_node query
    strcpy(buf, "d1:ad2:id20:abcdefghij01234567896:target20:mnopqrstuvwxyz123456e"
           "1:q9:find_node1:t2:aa1:y1:qe");
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode(&msg, buf, slen));

    // find_node response
    strcpy(buf, "d1:rd2:id20:0123456789abcdefghij5:nodesl"
           "20:abcdefghij012345678913:192.168.168.15:12120"
           "20:mnopqrstuvwxyz12345613:192.168.168.25:12121e"
           "e1:t2:aa1:y1:re");
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode(&msg, buf, slen));

    // bogus find_node response
    strcpy(buf, "d1:rd2:id20:0123456789abcdefghij5:nodesl"
           "4:abcd13:192.168.168.15:12120ee1:t2:aa1:y1:re");
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(!benc_decode(&msg, buf, slen));

    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
