/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#ifndef QUEUE_H
#define QUEUE_H

/**
 * A ring buffer FIFO queue.
 *
 * Inspired from https://stackoverflow.com/a/13888143/421846.
 * Another cool implementation: http://www.martinbroadhurst.com/cirque-in-c.html.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum queue_state {
    QUEUE_STATE_OK,
    QUEUE_STATE_EMPTY,
    QUEUE_STATE_FULL,
};

#define QUEUE_BIT_LEN(len) (size_t)1<<(len)

#define QUEUE_GENERATE(name, type, len)                   \
    typedef struct {                                      \
        type     *entries[QUEUE_BIT_LEN(len)];            \
        uint32_t  head    : len;                          \
        uint32_t  tail    : len;                          \
        uint32_t  is_full : 1;                            \
    } name;                                               \
    QUEUE_GENERATE_INIT(name)                             \
    QUEUE_GENERATE_PUT(name)                              \
    QUEUE_GENERATE_GET(name, type)                        \
    QUEUE_GENERATE_STATUS(name)

#define QUEUE_GENERATE_INIT(name)                                   \
static inline void name##_init(name *q)                             \
{                                                                   \
    memset(q, 0, sizeof(*q));                                       \
}

#define QUEUE_GENERATE_PUT(name)                        \
static inline bool name##_put(name *q, void *elt)                   \
{                                                                   \
    if (q->is_full)                                                 \
        return false;                                               \
    q->entries[q->tail] = elt;                                      \
    q->tail++;                                                      \
    if (q->tail == q->head)                                         \
        q->is_full = true;                                          \
    return true;                                                    \
}

#define QUEUE_GENERATE_GET(name, type)           \
static inline type *name##_get(name *q)                             \
{                                                                   \
    if (q->head == q->tail) {                                       \
        if (q->is_full)                                             \
            q->is_full = false;                                     \
        else                                                        \
            return NULL;                                            \
    }                                                               \
    return q->entries[q->head++];                                   \
}

#define QUEUE_GENERATE_STATUS(name)     \
static inline enum queue_state name##_status(name *q)               \
{                                                                   \
    if (q->is_full)                                                 \
        return QUEUE_STATE_FULL;                                    \
    else if (q->head == q->tail)                                    \
        return QUEUE_STATE_EMPTY;                                   \
    else                                                            \
        return QUEUE_STATE_OK;                                      \
}


#endif /* QUEUE_H */
