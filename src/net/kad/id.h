/* Copyright (c) 2020 Foudil Brétel.  All rights reserved. */
#ifndef ID_H
#define ID_H

#include "utils/byte_array.h"

#define KAD_RPC_MSG_TX_ID_LEN 2

/* Byte arrays are not affected by endian issues.
   http://stackoverflow.com/a/4523537/421846 */
BYTE_ARRAY_GENERATE(kad_guid, KAD_GUID_SPACE_IN_BYTES)

BYTE_ARRAY_GENERATE(kad_rpc_msg_tx_id, KAD_RPC_MSG_TX_ID_LEN)


#endif /* ID_H */
