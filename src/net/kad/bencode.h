/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <stdbool.h>
#include "net/kad/rpc.h"

bool benc_decode(struct kad_rpc_msg *msg, const char buf[], const size_t slen);
