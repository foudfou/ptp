/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include <netinet/in.h>
#include "log.h"
#include "net/kad/req_lru.h"
#include "net/kad/rpc.h"

int main ()
{
    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_CRIT)));
    struct kad_ctx ctx = {0};
    struct req_lru reqs_out = {0};
    ctx.reqs_out = &reqs_out;
    assert(kad_rpc_init(&ctx, NULL) == 0);

    assert(0 == node_heap_cmp(
               &(struct kad_node_lookup){.target = {.bytes = {[0]=0xff, [1]=0x04}},
                       .id = {.bytes = {[0]=0xff, [1]=0x04}}},
               &(struct kad_node_lookup){.target = {.bytes = {[0]=0xff, [1]=0x04}},
                       .id = {.bytes = {[0]=0xff, [1]=0x04}}}));
    assert(INT_MAX == node_heap_cmp(
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
    struct sockaddr_in6 *sa = (struct sockaddr_in6*)&ss;
    sa->sin6_family=AF_INET6; sa->sin6_port=htons(0x88b8);
    memcpy(sa->sin6_addr.s6_addr, (unsigned char[]){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, sizeof(struct in6_addr));
    assert(!kad_rpc_handle(&ctx, &ss, "", 0, &rsp));
    assert(rsp.pos != 0);
    iobuf_reset(&rsp);

    char buf[] = "d1:t2:aa1:y1:r1:rd2:id20:\x17""E\xc4\xed\xca\x16" \
        "3\xf0Q\x8e\x1f""6\n\xc7\xe1\xad'A\x86""3ee";
    assert(!kad_rpc_handle(&ctx, &ss, buf, 47, &rsp));
    assert(rsp.pos == 0);
    iobuf_reset(&rsp);

    kad_rpc_terminate(&ctx, NULL);
    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
