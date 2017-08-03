/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "net/kad/bencode.c"        // testing static functions
#include "net/kad/rpc.h"
#include "data.h"

int main ()
{
    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_CRIT)));
    struct kad_ctx ctx = {0};
    assert(kad_rpc_init(&ctx));

    struct iobuf rsp = {0};

    assert(kad_rpc_handle(&ctx, "::1", "35000", "", 0, &rsp) == -1);
    assert(rsp.pos != 0);
    iobuf_reset(&rsp);

    char buf[] = "d1:t2:aa1:y1:r1:rd2:id20:\x17""E\xc4\xed\xca\x16" \
        "3\xf0Q\x8e\x1f""6\n\xc7\xe1\xad'A\x86""3ee";
    assert(kad_rpc_handle(&ctx, "::1", "35000", buf, 47, &rsp) == -1);
    assert(rsp.pos == 0);
    iobuf_reset(&rsp);

    kad_rpc_terminate(&ctx);
    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
