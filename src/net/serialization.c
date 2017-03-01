/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include "serialization.h"

union u32 u32_hton(union u32 in)
{
    union u32 out;
    out.db[0] = (in.dd & 0xff000000) >> 24;
    out.db[1] = (in.dd & 0x00ff0000) >> 16;
    out.db[2] = (in.dd & 0x0000ff00) >> 8;
    out.db[3] = (in.dd & 0x000000ff);
    return out;
}

union u32 u32_ntoh(union u32 in)
{
    union u32 out;
    out.dd  = (uint64_t)in.db[0] << 24;
    out.dd |= (uint64_t)in.db[1] << 16;
    out.dd |= (uint64_t)in.db[2] << 8;
    out.dd |= (uint64_t)in.db[3];
    return out;
}

union u64 u64_hton(union u64 in)
{
    union u64 out;
    out.db[0] = (in.dd & 0xff00000000000000) >> 56;
    out.db[1] = (in.dd & 0x00ff000000000000) >> 48;
    out.db[2] = (in.dd & 0x0000ff0000000000) >> 40;
    out.db[3] = (in.dd & 0x000000ff00000000) >> 32;
    out.db[4] = (in.dd & 0x00000000ff000000) >> 24;
    out.db[5] = (in.dd & 0x0000000000ff0000) >> 16;
    out.db[6] = (in.dd & 0x000000000000ff00) >> 8;
    out.db[7] = (in.dd & 0x00000000000000ff);
    return out;
}

union u64 u64_ntoh(union u64 in)
{
    union u64 out;
    out.dd  = (uint64_t)in.db[0] << 56;
    out.dd |= (uint64_t)in.db[1] << 48;
    out.dd |= (uint64_t)in.db[2] << 40;
    out.dd |= (uint64_t)in.db[3] << 32;
    out.dd |= (uint64_t)in.db[4] << 24;
    out.dd |= (uint64_t)in.db[5] << 16;
    out.dd |= (uint64_t)in.db[6] << 8;
    out.dd |= (uint64_t)in.db[7];
    return out;
}
