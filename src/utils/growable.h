/* Copyright (c) 2025 Foudil Brétel.  All rights reserved. */
#ifndef GROWABLE_H
#define GROWABLE_H

/**
 * A growable append-only array of arbitrary data.
 *
 * Use _init() to initialize the array with a given capacity. But this is not
 * mandatory, as _append() will take care of growing the array to the
 * appropriate size, defaulting to @sz_init.
 *
 * The optional @cap_limit is the capacity limit which is only checked during
 * _init() and _grow().
 *
 * Used in growable structures like iobuf or binary heap.
 *
 * We could get rid of the append-only constraint by implementing iobuf_resize().
 *
 * There might be relunctance using realloc(3)
 * (http://www.iso-9899.info/wiki/Why_not_realloc) although it's quite
 * efficient actually
 * (http://blog.httrack.com/blog/2014/04/05/a-story-of-realloc-and-laziness/).
 */
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "utils/safer.h"

#define GROWABLE_GENERATE_BASIC(name, type, sz_init, factor, cap_limit) \
    GROWABLE_GENERATE_BASE(name, type)                                  \
    GROWABLE_GENERATE_INIT(name, type, cap_limit)                       \
    GROWABLE_GENERATE_GROW(name, type, sz_init, factor, cap_limit)      \
    GROWABLE_GENERATE_RESET(name)

#define GROWABLE_GENERATE(name, type, sz_init, factor, cap_limit)   \
    GROWABLE_GENERATE_BASIC(name, type, sz_init, factor, cap_limit) \
    GROWABLE_GENERATE_APPEND(name, type)

#define GROWABLE_GENERATE_BASE(name, type)      \
    struct name {                               \
        size_t  len;                            \
        size_t  cap;                            \
        type   *buf;                            \
    };

#define GROWABLE_GENERATE_INIT(name, type, cap_limit)     \
/**
 * Allocates the growable array.
 *
 * CAUTION: Consumers MUST free after use with _reset()
 */                                                                     \
static inline bool name##_init(struct name *g, size_t capa)             \
{                                                                       \
    if (capa > cap_limit)\
        return false;                                                   \
                                                                        \
    g->buf = calloc(capa, sizeof(type));                                \
    if (g->buf == NULL)                                                 \
        return false;                                                   \
    g->cap = capa;                                                      \
    g->len = 0;                                                         \
    return true;                                                        \
}

#define GROWABLE_GENERATE_GROW(name, type, sz_init, factor, cap_limit)  \
static inline bool                                                      \
name##_grow(struct name *g, const size_t len)                           \
{                                                                       \
    size_t needed = g->len + len;                                       \
                                                                        \
    if (cap_limit && needed > cap_limit) {                              \
        log_error("Can't grow over cap limit.");                        \
        return false;                                                   \
    }                                                                   \
                                                                        \
    size_t capa = g->cap;                                               \
    if (capa < sz_init)                                                 \
        capa = sz_init;                                                 \
                                                                        \
    while (capa < needed)                                               \
        capa *= factor;                                                 \
                                                                        \
    void *realloced = realloc(g->buf, sizeof(type) * capa);             \
    if (!realloced) {                                                   \
        log_perror(LOG_ERR, "Failed realloc: %s.", errno);              \
        return false;                                                   \
    }                                                                   \
    g->buf = realloced;                                                 \
    g->cap = capa;                                                      \
                                                                        \
    return true;                                                        \
}

#define GROWABLE_GENERATE_RESET(name)             \
static inline void name##_reset(struct name *g)   \
{                                                 \
    free_safer(g->buf);                           \
    g->len  = 0;                                  \
    g->cap = 0;                                   \
}

#define GROWABLE_GENERATE_APPEND(name, type)                        \
/**
 * Appends @len items from @data array to growable @g.
 *
 * CAUTION: appending more items than the @data array actually contains is
 * undefined behavior and a SECURITY issue.
 */                                                                 \
static inline bool                                                  \
name##_append(struct name *g, const type data[], const size_t len)  \
{                                                                   \
    if (len == 0)                                                   \
        return true;                                                \
                                                                    \
    /* log_debug("  buf_pos=%zu, len=%zu, capa=%zu", buf->pos, len, buf->capa); */ \
    if ((g->len + len > g->cap) && !name##_grow((g), len))          \
        return false;                                               \
    /* log_debug("  buf_pos=%zu, len=%zu, capa=%zu", buf->pos, len, buf->capa); */ \
                                                                    \
    memcpy(g->buf + g->len, data, sizeof(type) * len);              \
    g->len += len;                                                  \
                                                                    \
    /* LOG_FMT_HEX_DECL(bufx, buf->pos); */                         \
    /* log_fmt_hex(bufx, buf->pos, (unsigned char*)buf->buf); */    \
    /* log_debug("iobuf=%s", bufx); */                              \
                                                                    \
    return true;                                                    \
}



#endif /* GROWABLE_H */
