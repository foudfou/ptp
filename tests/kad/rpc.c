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

    kad_rpc_terminate(&ctx);
    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
