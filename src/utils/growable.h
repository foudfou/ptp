/* Copyright (c) 2025 Foudil Brétel.  All rights reserved. */
#ifndef GROWABLE_H
#define GROWABLE_H

/**
 * A growable append-only array of arbitrary data.
 *
 * Used in growable structures like iobuf or binary heap.
 */
#include <stdlib.h>
#include "log.h"
#include "utils/safer.h"

#define GROWABLE_FACTOR 2

#define GROWABLE_GENERATE(name, type)     \
    GROWABLE_GENERATE_BASE(name, type)    \
    GROWABLE_GENERATE_INIT(name, type)    \
    GROWABLE_GENERATE_GROW(name, type)    \
    GROWABLE_GENERATE_RESET(name)

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

#define GROWABLE_GENERATE_GROW(name, type)                              \
static inline bool                                                      \
name##_grow(struct name *g, const size_t len)                           \
{                                                                       \
    size_t needed = g->len + len;                                       \
                                                                        \
    size_t capa = g->cap;                                               \
    while (capa < needed)                                               \
        capa *= GROWABLE_FACTOR;                                        \
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
static inline void                                                  \
name##_append(struct name *g, const type *data, const size_t len)   \
{                                                                   \
    if ((g->len + len > g->cap) && !name##_grow((g), len))          \
        return false;                                               \
                                                                    \
    memcpy(g->buf + g->len, data, sizeof(type) * len);              \
    g->len += len;                                                  \
                                                                    \
    return true;                                                    \
}



#endif /* GROWABLE_H */
