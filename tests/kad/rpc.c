/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include <netinet/in.h>
#include "log.h"
#include "net/kad/rpc.h"

int main ()
{
    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_CRIT)));
    struct kad_ctx ctx = {0};
    assert(kad_rpc_init(&ctx, NULL));

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
