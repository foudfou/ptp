/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "utils/bits.h"

#define VAL1 (1U << 4)
#define VAL2 (1U << 6)

int main(void)
{
    unsigned int field = 0;
    BITS_SET(field, VAL1);
    assert(BITS_CHK(field, VAL1));

    assert(!BITS_CHK(field, VAL2));

    BITS_TGL(field, VAL1);
    assert(!BITS_CHK(field, VAL1));

    BITS_SET(field, VAL2);
    assert(BITS_CHK(field, VAL2));
    BITS_CLR(field, VAL2);
    assert(!BITS_CHK(field, VAL2));

    return 0;
}
