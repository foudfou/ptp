/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "log.h"
#include "net/util.h"
#include "net/kad/dht.h"
#include "net/kad/bencode/dht.h"
#include "data_dht.h"

#define BENC_PARSER_BUF_MAX 256


int main ()
{
    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_CRIT)));

    char buf[BENC_PARSER_BUF_MAX] = "\0";
    struct kad_dht_encoded encoded = {0};

    strcpy(buf, KAD_TEST_DHT);
    assert(benc_decode_dht(&encoded, buf, strlen(buf)));
    assert(kad_guid_eq(&encoded.self_id, &(kad_guid){.bytes = "0123456789abcdefghij", .is_set = true}));
    assert(encoded.nodes_len == 4);

    assert(kad_guid_eq(&encoded.nodes[0].id, &(kad_guid){.bytes = "abcdefghij0123456789", .is_set = true}));
    struct sockaddr_storage ss = {0};
    struct sockaddr_in *sa = (struct sockaddr_in*)&ss;
    sa->sin_family=AF_INET; sa->sin_addr.s_addr=htonl(0xc0a8a80f); sa->sin_port=htons(0x2f58);
    assert(sockaddr_storage_cmp4(&encoded.nodes[0].addr, &ss));

    assert(kad_guid_eq(&encoded.nodes[1].id, &(kad_guid){.bytes = "mnopqrstuvwxyz123456", .is_set = true}));
    memset(&ss, 0, sizeof(ss));
    sa->sin_family=AF_INET; sa->sin_addr.s_addr=htonl(0xc0a8a819); sa->sin_port=htons(0x2f59);
    assert(sockaddr_storage_cmp4(&encoded.nodes[1].addr, &ss));

    assert(kad_guid_eq(&encoded.nodes[2].id, &(kad_guid){.bytes = "9876543210jihgfedcba", .is_set = true}));
    struct sockaddr_in6 *sa6 = (struct sockaddr_in6*)&ss;
    sa6->sin6_family=AF_INET6; sa6->sin6_port=htons(0x0203);
    memcpy(sa6->sin6_addr.s6_addr, (unsigned char[]){1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, sizeof(struct in6_addr));
    assert(sockaddr_storage_cmp6(&encoded.nodes[2].addr, &ss));

    assert(kad_guid_eq(&encoded.nodes[3].id, &(kad_guid){.bytes = "654321zyxwvutsrqponm", .is_set = true}));
    memset(&ss, 0, sizeof(ss));
    sa6->sin6_family=AF_INET6; sa6->sin6_port=htons(0x0304);
    memcpy(sa6->sin6_addr.s6_addr, (unsigned char[]){1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0xaa}, sizeof(struct in6_addr));
    assert(sockaddr_storage_cmp6(&encoded.nodes[3].addr, &ss));


    struct iobuf dhtbuf = {0};
    memset(&encoded, 0, sizeof(encoded));
    encoded.self_id = (kad_guid){.bytes = "0123456789abcdefghij"};
    assert(benc_encode_dht(&dhtbuf, &encoded));
    assert(strncmp(dhtbuf.buf, "d2:id20:0123456789abcdefghij5:nodeslee", dhtbuf.pos) == 0);
    assert(dhtbuf.pos == 38);
    iobuf_reset(&dhtbuf);

    memset(&encoded, 0, sizeof(encoded));
    encoded.self_id = (kad_guid){.bytes = "0123456789abcdefghij"};
    sa->sin_family=AF_INET; sa->sin_addr.s_addr=htonl(0xc0a8a80f); sa->sin_port=htons(0x2f58);
    encoded.nodes[0] = (struct kad_node_info){
        .id = (kad_guid){.bytes = "abcdefghij0123456789", .is_set = true},
        .addr = ss, .addr_str = {0}
    };
    sa->sin_family=AF_INET; sa->sin_addr.s_addr=htonl(0xc0a8a819); sa->sin_port=htons(0x2f59);
    encoded.nodes[1] = (struct kad_node_info){
        .id = (kad_guid){.bytes = "mnopqrstuvwxyz123456", .is_set = true},
        .addr = ss, .addr_str = {0}
    };
    sa6->sin6_family=AF_INET6; sa6->sin6_port=htons(0x0203);
    memcpy(sa6->sin6_addr.s6_addr, (unsigned char[]){1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, sizeof(struct in6_addr));
    encoded.nodes[2] = (struct kad_node_info){
        .id = (kad_guid){.bytes = "9876543210jihgfedcba", .is_set = true},
        .addr = ss, .addr_str = {0}
    };
    sa6->sin6_family=AF_INET6; sa6->sin6_port=htons(0x0304);
    memcpy(sa6->sin6_addr.s6_addr, (unsigned char[]){1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0xaa}, sizeof(struct in6_addr));
    encoded.nodes[3] = (struct kad_node_info){
        .id = (kad_guid){.bytes = "654321zyxwvutsrqponm", .is_set = true},
        .addr = ss, .addr_str = {0}
    };
    encoded.nodes_len = 4;
    assert(benc_encode_dht(&dhtbuf, &encoded));
    assert(strncmp(dhtbuf.buf, KAD_TEST_DHT, dhtbuf.pos) == 0);
    assert(dhtbuf.pos == 178);
    iobuf_reset(&dhtbuf);


    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
