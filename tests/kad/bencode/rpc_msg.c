/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "log.h"
#include "net/socket.h"
#include "net/kad/rpc.h"
#include "net/kad/bencode/rpc_msg.h"
#include "data_rpc_msg.h"

#define BENC_PARSER_BUF_MAX 1400


static bool
check_encoded_msg(const struct kad_rpc_msg *msg, struct iobuf *msgbuf,
                  const char str[], const size_t str_len)
{
    return benc_encode_rpc_msg(msgbuf, msg)
        && (msgbuf->len == str_len)
        && (strncmp(msgbuf->buf, str, msgbuf->len) == 0);
}

static bool
check_msg_decode_and_reset(struct kad_rpc_msg *msg, struct iobuf *msgbuf)
{
    memset(msg, 0, sizeof(*msg));
    bool ret = benc_decode_rpc_msg(msg, msgbuf->buf, msgbuf->len);
    iobuf_reset(msgbuf);
    return ret;
}


int main ()
{
    kad_rpc_msg_tx_id TX_ID_CONST;
    memset(TX_ID_CONST.bytes, 'a', sizeof_field(kad_rpc_msg_tx_id, bytes));
    TX_ID_CONST.is_set = true;

    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_CRIT)));


    // Message decoding

    char buf[BENC_PARSER_BUF_MAX] = "\0";

    struct kad_rpc_msg msg = {0};

    /* log_info("__struct benc_literal: %zu, struct benc_node: %zu", sizeof(struct benc_literal), sizeof(struct benc_node)); */
    strcpy(buf, KAD_TEST_ERROR);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode_rpc_msg(&msg, buf, strlen(buf)));
    assert(kad_rpc_msg_tx_id_eq(&msg.tx_id, &TX_ID_CONST));
    assert(msg.type == KAD_RPC_TYPE_ERROR);
    assert(msg.err_code == 201);
    assert(strcmp(msg.err_msg, "A Generic Error Ocurred") == 0);

    strcpy(buf, KAD_TEST_PING_QUERY);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode_rpc_msg(&msg, buf, strlen(buf)));
    assert(kad_rpc_msg_tx_id_eq(&msg.tx_id, &TX_ID_CONST));
    assert(msg.type == KAD_RPC_TYPE_QUERY);
    assert(msg.meth == KAD_RPC_METH_PING);
    assert(kad_guid_eq(&msg.node_id, &(kad_guid){.bytes = "abcdefghij0123456789", .is_set = true}));

    strcpy(buf, KAD_TEST_PING_RESPONSE);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode_rpc_msg(&msg, buf, strlen(buf)));
    assert(kad_rpc_msg_tx_id_eq(&msg.tx_id, &TX_ID_CONST));
    assert(msg.type == KAD_RPC_TYPE_RESPONSE);
    assert(msg.meth == KAD_RPC_METH_NONE);
    assert(kad_guid_eq(&msg.node_id, &(kad_guid){.bytes = "mnopqrstuvwxyz123456", .is_set = true}));

    strcpy(buf, KAD_TEST_PING_RESPONSE_BIN_ID);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode_rpc_msg(&msg, buf, strlen(buf)));
    assert(kad_guid_eq(&msg.node_id, &(kad_guid){.bytes = "\x17\x45\xc4\xed" \
                    "\xca\x16\x33\xf0\x51\x8e\x1f\x36\x0a\xc7\xe1\xad" \
                    "\x27\x41\x86\x33", .is_set = true}));

    strcpy(buf, KAD_TEST_FIND_NODE_QUERY_BOGUS);
    memset(&msg, 0, sizeof(msg));
    assert(!benc_decode_rpc_msg(&msg, buf, strlen(buf)));

    strcpy(buf, KAD_TEST_FIND_NODE_QUERY);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode_rpc_msg(&msg, buf, strlen(buf)));
    assert(kad_rpc_msg_tx_id_eq(&msg.tx_id, &TX_ID_CONST));
    assert(msg.type == KAD_RPC_TYPE_QUERY);
    assert(msg.meth == KAD_RPC_METH_FIND_NODE);
    assert(kad_guid_eq(&msg.node_id, &(kad_guid){.bytes = "abcdefghij0123456789", .is_set = true}));
    assert(kad_guid_eq(&msg.target, &(kad_guid){.bytes = "mnopqrstuvwxyz123456", .is_set = true}));

    strcpy(buf, KAD_TEST_FIND_NODE_RESPONSE);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode_rpc_msg(&msg, buf, strlen(buf)));
    assert(kad_rpc_msg_tx_id_eq(&msg.tx_id, &TX_ID_CONST));
    assert(msg.type == KAD_RPC_TYPE_RESPONSE);
    assert(msg.meth == KAD_RPC_METH_NONE);
    assert(kad_guid_eq(&msg.node_id, &(kad_guid){.bytes = "0123456789abcdefghij", .is_set = true}));
    assert(msg.nodes_len == 2);
    assert(kad_guid_eq(&msg.nodes[0].id, &(kad_guid){.bytes = "abcdefghij0123456789", .is_set = true}));
    struct sockaddr_storage ss = {0};
    struct sockaddr_in *sa = (struct sockaddr_in*)&ss;
    sa->sin_family=AF_INET; sa->sin_addr.s_addr=htonl(0xc0a8a80f); sa->sin_port=htons(0x2f58);
    assert(sockaddr_storage_eq(&msg.nodes[0].addr, &ss));
    assert(kad_guid_eq(&msg.nodes[1].id, &(kad_guid){.bytes = "mnopqrstuvwxyz123456", .is_set = true}));
    memset(&ss, 0, sizeof(ss));
    sa->sin_family=AF_INET; sa->sin_addr.s_addr=htonl(0xc0a8a819); sa->sin_port=htons(0x2f59);
    assert(sockaddr_storage_eq(&msg.nodes[1].addr, &ss));

    strcpy(buf, KAD_TEST_FIND_NODE_RESPONSE_IP6);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode_rpc_msg(&msg, buf, strlen(buf)));
    assert(kad_rpc_msg_tx_id_eq(&msg.tx_id, &TX_ID_CONST));
    assert(msg.type == KAD_RPC_TYPE_RESPONSE);
    assert(msg.meth == KAD_RPC_METH_NONE);
    assert(kad_guid_eq(&msg.node_id, &(kad_guid){.bytes = "0123456789abcdefghij", .is_set = true}));
    assert(msg.nodes_len == 2);
    assert(kad_guid_eq(&msg.nodes[0].id, &(kad_guid){.bytes = "abcdefghij0123456789", .is_set = true}));
    memset(&ss, 0, sizeof(ss));
    struct sockaddr_in6 *sa6 = (struct sockaddr_in6*)&ss;
    sa6->sin6_family=AF_INET6; sa6->sin6_port=htons(0x0203);
    memcpy(sa6->sin6_addr.s6_addr, (unsigned char[]){1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, sizeof(struct in6_addr));
    assert(sockaddr_storage_eq(&msg.nodes[0].addr, &ss));
    assert(kad_guid_eq(&msg.nodes[1].id, &(kad_guid){.bytes = "mnopqrstuvwxyz123456", .is_set = true}));
    memset(&ss, 0, sizeof(ss));
    sa6->sin6_family=AF_INET6; sa6->sin6_port=htons(0x0304);
    memcpy(sa6->sin6_addr.s6_addr, (unsigned char[]){1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0xaa}, sizeof(struct in6_addr));
    assert(sockaddr_storage_eq(&msg.nodes[1].addr, &ss));

    strcpy(buf, KAD_TEST_FIND_NODE_RESPONSE_BOGUS);
    memset(&msg, 0, sizeof(msg));
    assert(benc_decode_rpc_msg(&msg, buf, strlen(buf)));
    assert(msg.nodes_len == 0);
    assert(kad_guid_eq(&msg.nodes[0].id, &(kad_guid){0}));


    // Message encoding

    struct iobuf msgbuf = {0};

    // KAD_TEST_ERROR
    memset(&msg, 0, sizeof(msg));
    /* set_expected_tx_id(&msg.tx_id); */
    msg.tx_id = TX_ID_CONST;
    msg.type = KAD_RPC_TYPE_ERROR;
    msg.err_code = 201;
    strcpy(msg.err_msg, "A Generic Error Occurred");
    assert(check_encoded_msg(&msg, &msgbuf, "d"
                             "1:e" "li201e" "24:A Generic Error Occurrede"
                             "1:t2:aa" "1:y1:e" "e",
                             52));
    assert(check_msg_decode_and_reset(&msg, &msgbuf));

    // KAD_TEST_PING_QUERY
    memset(&msg, 0, sizeof(msg));
    msg.tx_id = TX_ID_CONST;
    msg.type = KAD_RPC_TYPE_QUERY;
    msg.meth = KAD_RPC_METH_PING;
    msg.node_id = (kad_guid){.bytes = "abcdefghij0123456789"};
    assert(check_encoded_msg(&msg, &msgbuf, "d1:a" "d2:id"
                             "20:abcdefghij0123456789e" "1:q4:ping" "1:t2:aa1:y1:q" "e",
                             56));
    assert(check_msg_decode_and_reset(&msg, &msgbuf));

    // KAD_TEST_PING_RESPONSE
    memset(&msg, 0, sizeof(msg));
    msg.tx_id = TX_ID_CONST;
    msg.type = KAD_RPC_TYPE_RESPONSE;
    msg.meth = KAD_RPC_METH_PING;
    msg.node_id = (kad_guid){.bytes = "mnopqrstuvwxyz123456"};
    assert(check_encoded_msg(&msg, &msgbuf, "d1:rd2:id"
                             "20:mnopqrstuvwxyz123456e" "1:t2:aa1:y1:r" "e",
                             47));
    assert(check_msg_decode_and_reset(&msg, &msgbuf));

    // KAD_TEST_FIND_NODE_QUERY
    memset(&msg, 0, sizeof(msg));
    msg.tx_id = TX_ID_CONST;
    msg.type = KAD_RPC_TYPE_QUERY;
    msg.meth = KAD_RPC_METH_FIND_NODE;
    msg.node_id = (kad_guid){.bytes = "abcdefghij0123456789"};
    msg.target = (kad_guid){.bytes = "mnopqrstuvwxyz123456"};
    assert(check_encoded_msg(&msg, &msgbuf, "d1:ad"
                             "2:id20:abcdefghij0123456789"
                             "6:target20:mnopqrstuvwxyz123456"
                             "e1:q9:find_node" "1:t2:aa1:y1:qe",
                             92));
    assert(check_msg_decode_and_reset(&msg, &msgbuf));

    // KAD_TEST_FIND_NODE_RESPONSE
    memset(&msg, 0, sizeof(msg));
    msg.tx_id = TX_ID_CONST;
    msg.type = KAD_RPC_TYPE_RESPONSE;
    msg.meth = KAD_RPC_METH_FIND_NODE;
    msg.node_id = (kad_guid){.bytes = "0123456789abcdefghij"};
    memset(&ss, 0, sizeof(ss));
    msg.nodes[0] = (struct kad_node_info){
        .id = {.bytes = "abcdefghij0123456789"}, {0}, {0}};
    sa->sin_family=AF_INET; sa->sin_port=htons(0x2f58);
    sa->sin_addr.s_addr=htonl(0xc0a8a80f); sa->sin_port=htons(0x2f58);
    msg.nodes[0].addr = ss;
    msg.nodes[1] = (struct kad_node_info){
        .id = {.bytes = "mnopqrstuvwxyz123456"}, {0}, {0}};
    sa->sin_addr.s_addr=htonl(0xc0a8a819); sa->sin_port=htons(0x2f59);
    msg.nodes[1].addr = ss;
    msg.nodes_len = 2;
    assert(check_encoded_msg(&msg, &msgbuf, "d1:rd2:id20:0123456789abcdefghij5:nodesl"
                             "26:abcdefghij0123456789\xc0\xa8\xa8\x0f\x2f\x58"
                             "26:mnopqrstuvwxyz123456\xc0\xa8\xa8\x19\x2f\x59"
                             "ee1:t2:aa1:y1:re",
                             114));
    assert(check_msg_decode_and_reset(&msg, &msgbuf));

    // round-trip serialization tests
    struct {
        const char *data;
        enum kad_rpc_type type;
        enum kad_rpc_meth meth;
        unsigned long long err_code;
    } roundtrip_tests[] = {
        {KAD_TEST_ERROR, KAD_RPC_TYPE_ERROR, KAD_RPC_METH_NONE, 201},
        {KAD_TEST_PING_QUERY, KAD_RPC_TYPE_QUERY, KAD_RPC_METH_PING, 0},
        {KAD_TEST_FIND_NODE_QUERY, KAD_RPC_TYPE_QUERY, KAD_RPC_METH_FIND_NODE, 0},
        {NULL, 0, 0, 0}
    };

    struct iobuf roundtrip_buf = {0};
    assert(iobuf_init(&roundtrip_buf, 256));

    for (size_t i = 0; roundtrip_tests[i].data; i++) {
        strcpy(buf, roundtrip_tests[i].data);
        memset(&msg, 0, sizeof(msg));
        assert(benc_decode_rpc_msg(&msg, buf, strlen(buf)));
        assert(benc_encode_rpc_msg(&roundtrip_buf, &msg));
        memset(&msg, 0, sizeof(msg));
        assert(benc_decode_rpc_msg(&msg, roundtrip_buf.buf, roundtrip_buf.len));
        assert(msg.type == roundtrip_tests[i].type);
        if (roundtrip_tests[i].meth != KAD_RPC_METH_NONE)
            assert(msg.meth == roundtrip_tests[i].meth);
        if (roundtrip_tests[i].err_code != 0)
            assert(msg.err_code == roundtrip_tests[i].err_code);
        iobuf_reset(&roundtrip_buf);
    }

    iobuf_reset(&roundtrip_buf);

    // dictionary key ordering tests
    memset(&msg, 0, sizeof(msg));
    msg.tx_id = TX_ID_CONST;
    msg.type = KAD_RPC_TYPE_ERROR;
    msg.err_code = 201;
    strcpy(msg.err_msg, "Test Error");

    struct iobuf order_buf = {0};
    assert(iobuf_init(&order_buf, 256));
    assert(benc_encode_rpc_msg(&order_buf, &msg));

    const char *serialized = order_buf.buf;
    const char *key_t = strstr(serialized, "1:t");
    const char *key_y = strstr(serialized, "1:y");
    const char *key_e = strstr(serialized, "1:e");
    assert(key_t && key_y && key_e);
    assert(key_e < key_t);
    assert(key_t < key_y);

    iobuf_reset(&order_buf);

    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
