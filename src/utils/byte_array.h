/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef BYTE_ARRAY_H
#define BYTE_ARRAY_H

/**
 * A byte array which accepts 0x0 as a valid value.
 *
 * Note the order of bits inside bytes is considered the same as for bytes
 * themselves, ascending. For ex. changing bit 8 (starting from 0) actually
 * changes the MSB of byte 1.
 */
#include <limits.h>   // CHAR_BIT
#include <stdbool.h>
#include <string.h>
#include "utils/bits.h"

/**
 * Fetching the size of a struct's field.
 *
 * https://stackoverflow.com/a/3553321/421846
 */
#define sizeof_field(type, field) sizeof(((type *)0)->field)

#if defined(__clang__)
#define __nonstring
#elif defined(__GNUC__)
#define __nonstring __attribute__((__nonstring__))
#endif

#define BYTE_ARRAY_GENERATE(name, len)           \
    typedef struct {                             \
        unsigned char bytes[len] __nonstring;    \
        bool          is_set;                    \
    } name;                                      \
    BYTE_ARRAY_GENERATE_SET(name, len)           \
    BYTE_ARRAY_GENERATE_EQ(name, len)            \
    BYTE_ARRAY_GENERATE_RESET(name, len)         \
    BYTE_ARRAY_GENERATE_SETBIT(name, len)        \
    BYTE_ARRAY_GENERATE_XOR(name, len)

#define BYTE_ARRAY_INIT(typ, ary) ary = (typ){.bytes = {0}, .is_set = false}
#define BYTE_ARRAY_COPY(dst, src) dst = src
#define BYTE_ARRAY_CMP(a, b, len) memcmp(&(a)->bytes, &(b)->bytes, len)

#define BYTE_ARRAY_GENERATE_SET(type, len)      \
/**
 * Set a byte array with a given value.
 */                                                                 \
static inline void type##_set(type *ary, const unsigned char val[]) \
{                                                                   \
    memcpy(&ary->bytes, val, len);                                  \
    ary->is_set = true;                                             \
}

#define BYTE_ARRAY_GENERATE_EQ(type, len)       \
/**
 * Compares two byte arrays for equality.
 */                                                                     \
static inline bool type##_eq(const type *ary1, const type *ary2)        \
{                                                                       \
    return (ary1 && ary2) &&                                            \
        (ary1->is_set == ary2->is_set) &&                               \
        (memcmp(&ary1->bytes, &ary2->bytes, len) == 0);                 \
}

#define BYTE_ARRAY_GENERATE_RESET(type, len)   \
/**
 * Resets byte array.
 */                                                                     \
static inline void type##_reset(type *ary)                              \
{                                                                       \
    for (size_t i = 0; i < len; ++i) {                                  \
        ary->bytes[i] = 0;                                              \
    }                                                                   \
    ary->is_set = false;                                                \
}

#define BYTE_ARRAY_GENERATE_SETBIT(type, len)   \
/**
 * Sets the @nth bit of byte array. That's counting bits in same order as
 * bytes, "from left to right", _not_ from less to most significant bits. The
 * first bit is nth=0.
 */                                                                     \
static inline bool type##_setbit(type *ary, const size_t nth)           \
{                                                                       \
    if (nth >= len * 8)                                                 \
        return false;                                                   \
    size_t n = nth / 8;                                                 \
    size_t k = nth % 8;                                                 \
    BITS_SET(ary->bytes[n], 0x80 >> k);                                 \
    return true;                                                        \
}

#define BYTE_ARRAY_GENERATE_XOR(type, len)      \
/**
 * Xor two byte arrays and put the result into @out.
 */                                                                     \
static inline void type##_xor(type *out, const type *id1, const type *id2) \
{                                                                       \
    for (int i = 0; i < len; ++i) {                                     \
        out->bytes[i] = id1->bytes[i] ^ id2->bytes[i];                  \
    }                                                                   \
}

static inline unsigned cntl0(unsigned char x)
{
    static const unsigned char clz_lookup[16] = {4, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char upper = x >> 4;
    unsigned char lower = x & 0x0F;
    return upper ? clz_lookup[upper] : 4 + clz_lookup[lower];
}

/* Count leading zeros */
static inline unsigned clz(unsigned char n) {
    if (n == 0)
        return sizeof(n) * CHAR_BIT;
#if defined(__clang__)
    return __builtin_clzs((unsigned short)n << sizeof(n) * CHAR_BIT);
#elif defined(__GNUC__)
    return __builtin_clz((unsigned int)n << (sizeof(unsigned int) - 1) * CHAR_BIT);
#else
    return cntl0(n);
#endif
}

#endif /* BYTE_ARRAY_H */
