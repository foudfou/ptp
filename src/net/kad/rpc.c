/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include "log.h"
#include "utils/u64.h"
#include "net/kad/bencode.h"
#include "net/kad/rpc.h"
#include <string.h>

bool kad_rpc_parse(const char host[], const char service[],
                   const char buf[], const size_t slen)
{
    (void)host; (void)service; // FIXME:
    struct kad_rpc_msg msg = {0};
    return benc_decode(&msg, buf, slen);
}
