/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef BYTE_ARRAY_H
#define BYTE_ARRAY_H

/**
 * A byte array which accepts 0x0 as a valid value.
 */
#include <stdbool.h>
#include <string.h>

/**
 * Fetching the size of a struct's field.
 *
 * https://stackoverflow.com/a/3553321/421846
 */
#define sizeof_field(type, field) sizeof(((type *)0)->field)

#define BYTE_ARRAY_GENERATE(name, len)           \
    typedef struct {                             \
        unsigned char bytes[len];                \
        bool          is_set;                    \
    } name;                                      \
    BYTE_ARRAY_GENERATE_SET(name, len)           \
    BYTE_ARRAY_GENERATE_EQ(name, len)

#define BYTE_ARRAY_INIT(typ, ary) ary = (typ){.bytes = {0}, .is_set = false}
#define BYTE_ARRAY_COPY(dst, src) dst = src

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
    return (ary1->is_set == ary2->is_set)                               \
        && (memcmp(&ary1->bytes, &ary2->bytes, len) == 0);              \
}

#endif /* BYTE_ARRAY_H */
