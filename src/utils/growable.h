/* Copyright (c) 2025 Foudil Brétel.  All rights reserved. */
#ifndef GROWABLE_H
#define GROWABLE_H

/**
 * A growable append-only array of arbitrary data.
 *
 * Use x_init() to initialize the array with a given capacity. But this is not
 * mandatory, as x_append() will take care of growing the array to the
 * appropriate size, defaulting to @sz_init.
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

#define GROWABLE_FACTOR 2

// FIXME add optional limit
#define GROWABLE_GENERATE(name, type, sz_init, factor)     \
    GROWABLE_GENERATE_BASE(name, type)                     \
    GROWABLE_GENERATE_INIT(name, type)                     \
    GROWABLE_GENERATE_GROW(name, type, sz_init, factor)    \
    GROWABLE_GENERATE_RESET(name)
//  GROWABLE_GENERATE_APPEND(name, type) implement on demand

#define GROWABLE_GENERATE_BASE(name, type)                              \
    struct name {                                                       \
        size_t  len;                                                    \
        size_t  cap;                                                    \
        type   *buf;                                                    \
    };

#define GROWABLE_GENERATE_INIT(name, type)      \
/**
 * Allocates the growable array.
 *
 * CAUTION: Consumers MUST free after use with _reset()
 */                                                                     \
static inline bool name##_init(struct name *g, size_t capa)             \
{                                                                       \
    g->buf = calloc(capa, sizeof(type));                                \
    if (g->buf == NULL)                                                 \
        return false;                                                   \
    g->cap = capa;                                                      \
    g->len = 0;                                                         \
    return true;                                                        \
}

#define GROWABLE_GENERATE_GROW(name, type, sz_init, factor)             \
static inline bool                                                      \
name##_grow(struct name *g, const size_t len)                           \
{                                                                       \
    size_t needed = g->len + len;                                       \
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
static inline bool                                                  \
name##_append(struct name *g, const type *data, const size_t len)   \
{                                                                   \
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
