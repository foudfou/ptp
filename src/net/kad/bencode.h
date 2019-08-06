/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#ifndef BENCODE_H
#define BENCODE_H
/**
 * Dirty KRPC bencoding de-/serializer, not a generic bencode implementation.
 */
#include <stdbool.h>
#include "net/iobuf.h"
#include "net/kad/rpc.h"

bool benc_decode(struct kad_rpc_msg *msg, const char buf[], const size_t slen);
bool benc_encode(const struct kad_rpc_msg *msg, struct iobuf *buf);

#endif /* BENCODE_H */
