/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "log.h"
#include "net/kad/dht.h"
#include "net/kad/bencode/dht.h"
#include "net/socket.h"
#include "utils/array.h"
#include "kad/test_util.h"
#include "data_dht.h"

#define BENC_PARSER_BUF_MAX 256

KAD_TEST_NODES_DECL;

int main ()
{
    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_CRIT)));

    char buf[BENC_PARSER_BUF_MAX] = "\0";
    struct kad_dht_encoded encoded = {0};

    strcpy(buf, KAD_TEST_DHT);
    assert(benc_decode_dht(&encoded, buf, strlen(buf)));
    assert(kad_guid_eq(&encoded.self_id, &(kad_guid){.bytes = "0123456789abcdefghij", .is_set = true}));
    assert(encoded.nodes_len == ARRAY_LEN(kad_test_nodes));

    for (size_t i=0; i<ARRAY_LEN(kad_test_nodes); i++) {
        assert(kad_node_info_equals(&encoded.nodes[i], &kad_test_nodes[i]));
    }

    struct iobuf dhtbuf = {0};
    memset(&encoded, 0, sizeof(encoded));
    encoded.self_id = (kad_guid){.bytes = "0123456789abcdefghij"};
    assert(benc_encode_dht(&dhtbuf, &encoded));
    assert(strncmp(dhtbuf.buf, "d2:id20:0123456789abcdefghij5:nodeslee", dhtbuf.pos) == 0);
    assert(dhtbuf.pos == 38);
    iobuf_reset(&dhtbuf);

    memset(&encoded, 0, sizeof(encoded));
    encoded.self_id = (kad_guid){.bytes = "0123456789abcdefghij"};

    for (size_t i=0; i<ARRAY_LEN(kad_test_nodes); i++) {
        kad_node_info_set(&encoded.nodes[i], &kad_test_nodes[i]);
    }
    encoded.nodes_len = ARRAY_LEN(kad_test_nodes);

    assert(benc_encode_dht(&dhtbuf, &encoded));
    assert(strncmp(dhtbuf.buf, KAD_TEST_DHT, dhtbuf.pos) == 0);
    assert(dhtbuf.pos == 178);
    iobuf_reset(&dhtbuf);


    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
