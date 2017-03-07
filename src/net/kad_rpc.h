/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef KAD_RPC_H
#define KAD_RPC_H

/**
 * KRPC Protocol as defined in http://www.bittorrent.org/beps/bep_0005.html.
 */
#include <netdb.h>
#include <stdbool.h>
#include <stdlib.h>
#include "iobuf.h"
#include "kad.h"


bool kad_rpc_parse(const char host[], const char service[],
                   const char buf[], const size_t slen);

#endif /* KAD_RPC_H */
