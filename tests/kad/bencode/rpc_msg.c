/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "log.h"
#include "net/kad/rpc.h"
#include "net/kad/bencode/rpc_msg.h"
#include "data.h"

#define BENC_PARSER_BUF_MAX 1400

bool check_encoded_msg(const struct kad_rpc_msg *msg, struct iobuf *msgbuf,
                       const char str[], const size_t str_len)
{
    return benc_encode_rpc_msg(msg, msgbuf)
        && (msgbuf->pos == str_len)
        && (strncmp(msgbuf->buf, str, msgbuf->pos) == 0);
}

bool check_msg_decode_and_reset(struct kad_rpc_msg *msg, struct iobuf *msgbuf)
{
    memset(msg, 0, sizeof(*msg));
    bool ret = benc_decode_rpc_msg(msg, msgbuf->buf, msgbuf->pos);
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

    struct kad_rpc_msg msg;
    KAD_RPC_MSG_INIT(msg);

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

    /* strcpy(buf, KAD_TEST_FIND_NODE_QUERY); */
    /* memset(&msg, 0, sizeof(msg)); */
    /* assert(benc_decode_rpc_msg(&msg, buf, strlen(buf))); */
    /* assert(kad_rpc_msg_tx_id_eq(&msg.tx_id, &TX_ID_CONST)); */
    /* assert(msg.type == KAD_RPC_TYPE_QUERY); */
    /* assert(msg.meth == KAD_RPC_METH_FIND_NODE); */
    /* assert(kad_guid_eq(&msg.node_id, &(kad_guid){.bytes = "abcdefghij0123456789", .is_set = true})); */
    /* assert(kad_guid_eq(&msg.target, &(kad_guid){.bytes = "mnopqrstuvwxyz123456", .is_set = true})); */

    /* strcpy(buf, KAD_TEST_FIND_NODE_RESPONSE); */
    /* memset(&msg, 0, sizeof(msg)); */
    /* assert(benc_decode_rpc_msg(&msg, buf, strlen(buf))); */
    /* assert(kad_rpc_msg_tx_id_eq(&msg.tx_id, &TX_ID_CONST)); */
    /* assert(msg.type == KAD_RPC_TYPE_RESPONSE); */
    /* assert(msg.meth == KAD_RPC_METH_NONE); */
    /* assert(kad_guid_eq(&msg.node_id, &(kad_guid){.bytes = "0123456789abcdefghij", .is_set = true})); */
    /* assert(msg.nodes_len == 2); */
    /* assert(strcmp(msg.nodes[0].host, "192.168.168.1") == 0); */
    /* assert(strcmp(msg.nodes[0].service, "12120") == 0); */
    /* assert(kad_guid_eq(&msg.nodes[1].id, &(kad_guid){.bytes = "mnopqrstuvwxyz123456", .is_set = true})); */
    /* assert(strcmp(msg.nodes[1].host, "192.168.168.2") == 0); */
    /* assert(strcmp(msg.nodes[1].service, "12121") == 0); */

    /* strcpy(buf, KAD_TEST_FIND_NODE_RESPONSE_BOGUS); */
    /* memset(&msg, 0, sizeof(msg)); */
    /* assert(!benc_decode_rpc_msg(&msg, buf, strlen(buf))); */


    // Message encoding

    /* The order of fields shouldn't matter. Our implementation is just
       deterministic. */

    struct iobuf msgbuf = {0};

    // KAD_TEST_ERROR
    KAD_RPC_MSG_INIT(msg);
    /* set_expected_tx_id(&msg.tx_id); */
    msg.tx_id = TX_ID_CONST;
    msg.type = KAD_RPC_TYPE_ERROR;
    msg.err_code = 201;
    strcpy(msg.err_msg, "A Generic Error Ocurred");
    assert(check_encoded_msg(&msg, &msgbuf, "d1:t2:aa1:y1:e1:eli201e"
                             "23:A Generic Error Ocurredee",
                             51));
    assert(check_msg_decode_and_reset(&msg, &msgbuf));

    // KAD_TEST_PING_QUERY
    KAD_RPC_MSG_INIT(msg);
    msg.tx_id = TX_ID_CONST;
    msg.type = KAD_RPC_TYPE_QUERY;
    msg.meth = KAD_RPC_METH_PING;
    msg.node_id = (kad_guid){.bytes = "abcdefghij0123456789"};
    assert(check_encoded_msg(&msg, &msgbuf, "d1:t2:aa1:y1:q1:q4:ping1:ad2:id"
                             "20:abcdefghij0123456789ee",
                             56));
    assert(check_msg_decode_and_reset(&msg, &msgbuf));

    // KAD_TEST_PING_RESPONSE
    KAD_RPC_MSG_INIT(msg);
    msg.tx_id = TX_ID_CONST;
    msg.type = KAD_RPC_TYPE_RESPONSE;
    msg.meth = KAD_RPC_METH_PING;
    msg.node_id = (kad_guid){.bytes = "mnopqrstuvwxyz123456"};
    assert(check_encoded_msg(&msg, &msgbuf, "d1:t2:aa1:y1:r1:rd2:id"
                             "20:mnopqrstuvwxyz123456ee",
                             47));
    assert(check_msg_decode_and_reset(&msg, &msgbuf));

    // KAD_TEST_FIND_NODE_QUERY
    KAD_RPC_MSG_INIT(msg);
    msg.tx_id = TX_ID_CONST;
    msg.type = KAD_RPC_TYPE_QUERY;
    msg.meth = KAD_RPC_METH_FIND_NODE;
    msg.node_id = (kad_guid){.bytes = "abcdefghij0123456789"};
    msg.target = (kad_guid){.bytes = "mnopqrstuvwxyz123456"};
    assert(check_encoded_msg(&msg, &msgbuf, "d1:t2:aa1:y1:q1:q9:find_node1:ad"
                             "6:target20:mnopqrstuvwxyz1234562:id"
                             "20:abcdefghij0123456789ee",
                             92));
    assert(check_msg_decode_and_reset(&msg, &msgbuf));

    // KAD_TEST_FIND_NODE_RESPONSE
    KAD_RPC_MSG_INIT(msg);
    msg.tx_id = TX_ID_CONST;
    msg.type = KAD_RPC_TYPE_RESPONSE;
    msg.meth = KAD_RPC_METH_FIND_NODE;
    msg.node_id = (kad_guid){.bytes = "0123456789abcdefghij"};
    msg.nodes[0] = (struct kad_node_info){
        .id = {.bytes = "abcdefghij0123456789"}, "192.168.168.15", "12120" };
    msg.nodes[1] = (struct kad_node_info){
        .id = {.bytes = "mnopqrstuvwxyz123456"}, "192.168.168.25", "12121" };
    msg.nodes_len = 2;
    assert(check_encoded_msg(&msg, &msgbuf,"d1:t2:aa1:y1:r1:rd5:nodesl"
                             "20:abcdefghij012345678914:192.168.168.155:12120"
                             "20:mnopqrstuvwxyz12345614:192.168.168.255:12121"
                             "e2:id20:0123456789abcdefghijee",
                             150));
    assert(check_msg_decode_and_reset(&msg, &msgbuf));

    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
