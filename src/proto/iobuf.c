/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <errno.h>
#include <string.h>
#include "log.h"
#include "utils/safe.h"
#include "proto/iobuf.h"

/**
 * For now, we only grow the buffer. But we could also iobuf_resize() instead.
 */
static bool iobuf_grow(struct iobuf *buf)
{
    size_t capa = buf->capa;
    if (capa < IOBUF_SIZE_INITIAL)
        capa = IOBUF_SIZE_INITIAL;
    else
        capa *= IOBUF_SIZE_FACTOR;
    buf->buf = realloc(buf->buf, capa);
    if (!buf->buf) {
        log_perror("Failed realloc: %s.", errno);
        return false;
    }
    buf->capa = capa;
    return true;
}

void iobuf_reset(struct iobuf *buf)
{
    safe_free(buf->buf);
    buf->pos  = 0;
    buf->capa = 0;
}

bool iobuf_append(struct iobuf *buf, const char *data, const size_t len)
{
    log_debug("  buf_pos=%zu, len=%zu, capa=%zu", buf->pos, len, buf->capa);
    if ((buf->pos + len > buf->capa) && !iobuf_grow(buf))
        return false;
    log_debug("  buf_pos=%zu, len=%zu, capa=%zu", buf->pos, len, buf->capa);

    memcpy(buf->buf + buf->pos, data, len);
    buf->pos += len;
    log_debug_hex(buf->buf, buf->pos);
    return true;

}
