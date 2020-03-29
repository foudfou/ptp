/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "utils/bitfield.h"

#include <stdio.h>
int main(void)
{
    bitfield arr[BITFIELD_RESERVE_BITS(130)] = {0x80000001, 0x12345678, 0xabcdef0, 0xffff0000, 0};
    assert(BITFIELD_GET(arr, 0) == 1);
    assert(BITFIELD_GET(arr, 1) == 0);
    assert(BITFIELD_GET(arr, 31) == 1);

    assert(BITFIELD_GET(arr, 6) == 0);
    BITFIELD_SET(arr, 6, 1);
    assert(BITFIELD_GET(arr, 6) == 1);
    int x = 2;            // the least significant bit is 0
    BITFIELD_SET(arr, 6, x);    // sets bit 6 to 0 because 2&1 is 0
    assert(BITFIELD_GET(arr, 6) == 0);
    BITFIELD_SET(arr, 6, ~x);  // sets bit 6 to 1 because ~2 is 1
    assert(BITFIELD_GET(arr, 6) == 1);


    return 0;
}
