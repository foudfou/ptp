/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include <netinet/in.h>
#include "log.h"
#include "kad/test_util.h"
#include "net/kad/req_lru.h"
#include "net/kad/rpc.c"

int main ()
{
    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_CRIT)));
    struct kad_ctx ctx = {0};
    struct list_item timers = LIST_ITEM_INIT(timers);
    ctx.timers = &timers;
    struct req_lru reqs_out = {0};
    ctx.reqs_out = &reqs_out;
    assert(kad_rpc_init(&ctx, NULL) == 0);

    assert(0 == node_heap_cmp(
               &(struct kad_node_lookup){.target = {.bytes = {[0]=0xff, [1]=0x04}},
                       .id = {.bytes = {[0]=0xff, [1]=0x04}}},
               &(struct kad_node_lookup){.target = {.bytes = {[0]=0xff, [1]=0x04}},
                       .id = {.bytes = {[0]=0xff, [1]=0x04}}}));
    assert(INT_MIN == node_heap_cmp(
               &(struct kad_node_lookup){.target = {.bytes = {[0]=0, [1]=0}},
                       .id = {.bytes = {[0]=0, [1]=0xff}}},
               &(struct kad_node_lookup){.target = {.bytes = {[0]=1, [1]=0}},
                       .id = {.bytes = {[0]=0, [1]=0xff}}}));
    assert(0 < node_heap_cmp(
               &(struct kad_node_lookup){.target = {.bytes = {[0]=0, [1]=0xff}},
                       .id = {.bytes = {[0]=0, [1]=1}}}, // fe
               &(struct kad_node_lookup){.target = {.bytes = {[0]=0, [1]=0xff}},
                       .id = {.bytes = {[0]=1, [1]=0}}})); // 1ff
    assert(0 > node_heap_cmp(
               &(struct kad_node_lookup){.target = {.bytes = {[0]=0, [1]=0xff}},
                       .id = {.bytes = {[0]=1, [1]=0}}},
               &(struct kad_node_lookup){.target = {.bytes = {[0]=0, [1]=0xff}},
                       .id = {.bytes = {[0]=0, [1]=1}}}));


    struct iobuf rsp = {0};

    struct sockaddr_storage ss = {0};
    struct sockaddr_in6 *sa6 = (struct sockaddr_in6*)&ss;
    sa6->sin6_family=AF_INET6; sa6->sin6_port=htons(0x88b8);
    memcpy(sa6->sin6_addr.s6_addr, (unsigned char[]){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, sizeof(struct in6_addr));
    assert(!kad_rpc_handle(&ctx, &ss, "", 0, &rsp));
    assert(rsp.len != 0);
    iobuf_reset(&rsp);

    char buf[] = "d1:t2:aa1:y1:r1:rd2:id20:\x17""E\xc4\xed\xca\x16" \
        "3\xf0Q\x8e\x1f""6\n\xc7\xe1\xad'A\x86""3ee";
    assert(kad_rpc_handle(&ctx, &ss, buf, 47, &rsp));
    assert(rsp.len == 0);
    iobuf_reset(&rsp);


    // find_node answer

    struct kad_rpc_query *q1 = calloc(1, sizeof(struct kad_rpc_query));
    assert(q1);
    assert(query_init(q1));
    *q1 = (struct kad_rpc_query){
        .msg = {
            .tx_id={"x1", true},
            .node_id={{0x1}, true},
            .type=KAD_RPC_TYPE_QUERY,
            .meth=KAD_RPC_METH_FIND_NODE,
            .target={{3}, true},
        }
    };
    struct kad_rpc_query *evicted;
    assert(req_lru_put(ctx.reqs_out, q1, &evicted));
    ctx.lookup.par[0] = q1;

    struct kad_node_info nodes[3] = {
        {{{0x2}, true}, {0}, {0}},
        {{{0x4}, true}, {0}, {0}},
        {{{0x6}, true}, {0}, {0}},
    };

    struct sockaddr_in *sa = (struct sockaddr_in*)&ss;
    sa->sin_family=AF_INET; sa->sin_port=htons(0x0016);
    sa->sin_addr.s_addr=htonl(0x01010101); nodes[0].addr = ss;
    sa->sin_addr.s_addr=htonl(0x01010200); nodes[1].addr = ss;
    sa->sin_addr.s_addr=htonl(0x01010201); nodes[2].addr = ss;

    struct kad_rpc_msg r1 = {
        .tx_id={"x1", true},
        .node_id={{0x1}, true},
        .type=KAD_RPC_TYPE_RESPONSE,
        .meth=KAD_RPC_METH_FIND_NODE,
        .nodes={nodes[0], nodes[1], nodes[2]},
        .nodes_len=3,
    };
    assert(kad_rpc_handle_response(&ctx, &r1));

    assert(list_count(&ctx.reqs_out->litems) == 0);
    size_t route_count = 0;
    for (size_t i = 0; i < KAD_GUID_SPACE_IN_BITS; i++)
        route_count += list_count(&ctx.routes->buckets[i]);
    assert(route_count == 4);
    assert(ctx.lookup.next.len == 3);
    assert(ctx.lookup.round == 1);


    kad_rpc_terminate(&ctx, NULL);
    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
