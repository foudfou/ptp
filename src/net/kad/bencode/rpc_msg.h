/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#ifndef BENCODE_RPC_MSG_H
#define BENCODE_RPC_MSG_H
/**
 * KRPC bencoding de-/serializer
 */
#include <stdbool.h>
#include "net/iobuf.h"
#include "net/kad/rpc.h"

bool benc_decode_rpc_msg(struct kad_rpc_msg *msg, const char buf[], const size_t slen);
bool benc_encode_rpc_msg(struct iobuf *buf, const struct kad_rpc_msg *msg);

#endif /* BENCODE_RPC_MSG_H */
