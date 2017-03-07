/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "net/kad_rpc.c"        // testing static functions

#define KAD_RPC_PARSER_BUF_MAX 1400

#include <limits.h>

int main ()
{
    char buf[KAD_RPC_PARSER_BUF_MAX] = "i-300e";
    size_t slen = strlen(buf);
    struct kad_rpc_parser parser;
    kad_rpc_parser_init(&parser, buf, slen);

    struct kad_rpc_val val;
    assert(kad_rpc_parse_int(&parser, &val));
    assert(val.i == -300);

    strcpy(buf, "i3.14e");
    slen = strlen(buf);
    kad_rpc_parser_init(&parser, buf, slen);
    assert(!kad_rpc_parse_int(&parser, &val));
    assert(parser.err);

    strcpy(buf, "i9223372036854775808e"); // overflow
    slen = strlen(buf);
    kad_rpc_parser_init(&parser, buf, slen);
    assert(!kad_rpc_parse_int(&parser, &val));
    assert(parser.err);

    strcpy(buf, "4:spam");
    slen = strlen(buf);
    kad_rpc_parser_init(&parser, buf, slen);
    assert(kad_rpc_parse_str(&parser, &val));
    assert(val.s.len == 4);
    assert(strncmp(val.s.p, "spam", val.s.len) == 0);
    assert(*parser.cur == '\0');
    assert(POINTER_OFFSET(parser.beg, parser.cur) == 6);

    strcpy(buf, "2:\x11\x22");
    slen = strlen(buf);
    kad_rpc_parser_init(&parser, buf, slen);
    assert(kad_rpc_parse_str(&parser, &val));
    assert(val.s.len == 2);
    assert(strncmp(val.s.p, "\x11\x22", val.s.len) == 0);
    assert(POINTER_OFFSET(parser.beg, parser.cur) == 4);

    strcpy(buf, "65535:anything");  // overflow
    slen = strlen(buf);
    kad_rpc_parser_init(&parser, buf, slen);
    assert(!kad_rpc_parse_str(&parser, &val));
    assert(parser.err);

    kad_rpc_parser_terminate(&parser);


    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_CRIT)));

    // fail: incomplete
    strcpy(buf, "d");
    slen = strlen(buf);
    assert(!kad_rpc_parse("127.0.0.1", "43999", buf, slen));

    // ok: void dict
    strcpy(buf, "de");
    slen = strlen(buf);
    assert(kad_rpc_parse("127.0.0.1", "43999", buf, slen));

    /* // fail: syntax */
    /* strcpy(buf, "dede"); */
    /* slen = strlen(buf); */
    /* assert(!kad_rpc_parse("127.0.0.1", "43999", buf, slen)); */

    // error
    strcpy(buf, "d1:eli201e23:A Generic Error Ocurrede1:t2:aa1:y1:ee");
    slen = strlen(buf);
    assert(kad_rpc_parse("127.0.0.1", "43999", buf, slen));

    // ping query
    strcpy(buf, "d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t2:aa1:y1:qe");
    slen = strlen(buf);
    assert(kad_rpc_parse("127.0.0.1", "43999", buf, slen));

    // ping response
    strcpy(buf, "d1:rd2:id20:mnopqrstuvwxyz123456e1:t2:aa1:y1:re");
    slen = strlen(buf);
    assert(kad_rpc_parse("127.0.0.1", "43999", buf, slen));

    // find_node query
    strcpy(buf, "d1:ad2:id20:abcdefghij01234567896:target20:mnopqrstuvwxyz123456e"
           "1:q9:find_node1:t2:aa1:y1:qe");
    slen = strlen(buf);
    assert(kad_rpc_parse("127.0.0.1", "43999", buf, slen));

    // find_node response
    strcpy(buf, "d1:rd2:id20:0123456789abcdefghij5:nodes9:def456...e1:t2:aa1:y1:re");
    slen = strlen(buf);
    assert(kad_rpc_parse("127.0.0.1", "43999", buf, slen));

    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
