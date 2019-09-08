/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
// FIXME: naming consistency with byte_array.h (underscore or not).
#ifndef BITFIELD_H
#define BITFIELD_H

/**
 * An efficient bit field.
 *
 * Bit fields are slightly different to byte arrays in that they focus on bits.
 *
 * Directly copied from https://stackoverflow.com/a/24753227/421846.
 * TODO our own implementation based on bits.h for consistency.
 */
#include <inttypes.h>

typedef uint32_t bitfield;

#define BITFIELD_RESERVE_BITS(n) (((n)+0x1f)>>5)
#define BITFIELD_DWORD_IDX(x) ((x)>>5)
#define BITFIELD_BIT_IDX(x) ((x)&0x1f)
#define BITFIELD_GET(array, index) (((array)[BITFIELD_DWORD_IDX(index)]>>BITFIELD_BIT_IDX(index))&1)
#define BITFIELD_SET(array, index, bit)                                 \
    (((bit)&1) ? ((array)[BITFIELD_DWORD_IDX(index)] |= UINT32_C(1)<<BITFIELD_BIT_IDX(index)) \
     : ((array)[BITFIELD_DWORD_IDX(index)] &= ~(UINT32_C(1)<<BITFIELD_BIT_IDX(index))) \
     , (void)0                                                          \
        )

#endif /* BITFIELD_H */
