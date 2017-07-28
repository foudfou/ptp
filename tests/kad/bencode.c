/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "net/kad/bencode.c"        // testing static functions
#include "net/kad/rpc.h"
#include "data.h"

#define BENC_PARSER_BUF_MAX 1400

int main ()
{
    // Basic parsing

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


    // Message decoding

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
    assert(msg.tx_id[0] == 'a' && msg.tx_id[1] == 'a');
    assert(msg.type == KAD_RPC_TYPE_ERROR);
    assert(msg.err_code == 201);
    assert(strcmp(msg.err_msg, "A Generic Error Ocurred") == 0);

    strcpy(buf, KAD_TEST_PING_QUERY);
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode(&msg, buf, slen));
    assert(msg.tx_id[0] == 'a' && msg.tx_id[1] == 'a');
    assert(msg.type == KAD_RPC_TYPE_QUERY);
    assert(msg.meth == KAD_RPC_METH_PING);
    assert(kad_guid_eq(&msg.node_id, &(kad_guid){.b = "abcdefghij0123456789"}));

    strcpy(buf, KAD_TEST_PING_RESPONSE);
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode(&msg, buf, slen));
    assert(msg.tx_id[0] == 'a' && msg.tx_id[1] == 'a');
    assert(msg.type == KAD_RPC_TYPE_RESPONSE);
    assert(msg.meth == KAD_RPC_METH_NONE);
    assert(kad_guid_eq(&msg.node_id, &(kad_guid){.b = "mnopqrstuvwxyz123456"}));

    strcpy(buf, KAD_TEST_FIND_NODE_QUERY);
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode(&msg, buf, slen));
    assert(msg.tx_id[0] == 'a' && msg.tx_id[1] == 'a');
    assert(msg.type == KAD_RPC_TYPE_QUERY);
    assert(msg.meth == KAD_RPC_METH_FIND_NODE);
    assert(kad_guid_eq(&msg.node_id, &(kad_guid){.b = "abcdefghij0123456789"}));
    assert(kad_guid_eq(&msg.target, &(kad_guid){.b = "mnopqrstuvwxyz123456"}));

    strcpy(buf, KAD_TEST_FIND_NODE_RESPONSE);
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode(&msg, buf, slen));
    assert(msg.tx_id[0] == 'a' && msg.tx_id[1] == 'a');
    assert(msg.type == KAD_RPC_TYPE_RESPONSE);
    assert(msg.meth == KAD_RPC_METH_NONE);
    assert(kad_guid_eq(&msg.node_id, &(kad_guid){.b = "0123456789abcdefghij"}));
    assert(msg.nodes_len == 2);
    assert(strcmp(msg.nodes[0].host, "192.168.168.1") == 0);
    assert(strcmp(msg.nodes[0].service, "12120") == 0);
    assert(kad_guid_eq(&msg.nodes[1].id, &(kad_guid){.b = "mnopqrstuvwxyz123456"}));
    assert(strcmp(msg.nodes[1].host, "192.168.168.2") == 0);
    assert(strcmp(msg.nodes[1].service, "12121") == 0);

    strcpy(buf, KAD_TEST_FIND_NODE_RESPONSE_BOGUS);
    slen = strlen(buf);
    memset(&msg, 0, sizeof(msg));
    assert(!benc_decode(&msg, buf, slen));


    iobuf_reset(&msgbuf);

    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
