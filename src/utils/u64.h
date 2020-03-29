/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef U64_H
#define U64_H

#include <stdint.h>
#include <inttypes.h>  // for PRIu64

#define IS_LITTLE_ENDIAN ((union u32){ .dd = 1 }.db[0])

union u32 {  // printf PRIu32
    uint32_t dd;
    uint8_t  db[4];
};

/* http://stackoverflow.com/a/13995796/421846
   http://stackoverflow.com/a/13352059/421846 */
union u64 {  // printf PRIu64
    uint64_t dd;
    uint32_t dw[2];
    uint8_t  db[8];
};

union u32 u32_hton(union u32 in);
union u32 u32_ntoh(union u32 in);
union u64 u64_hton(union u64 in);
union u64 u64_ntoh(union u64 in);

#endif /* U64_H */
