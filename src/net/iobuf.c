/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <errno.h>
#include <string.h>
#include "log.h"
#include "net/iobuf.h"
#include "utils/safer.h"

/**
 * For now, we only grow the buffer. But we could also iobuf_resize() instead.
 *
 * There might be relunctance using realloc(3)
 * (http://www.iso-9899.info/wiki/Why_not_realloc) although it's quite
 * efficient actually
 * (http://blog.httrack.com/blog/2014/04/05/a-story-of-realloc-and-laziness/).
 */
static bool iobuf_grow(struct iobuf *buf, const size_t len)
{
    size_t needed = buf->pos + len;

    size_t capa = buf->capa;
    if (capa < IOBUF_SIZE_INITIAL)
        capa = IOBUF_SIZE_INITIAL;

    while (capa < needed)
        capa *= IOBUF_SIZE_FACTOR;

    void *realloced = realloc(buf->buf, capa);
    if (!realloced) {
        log_perror(LOG_ERR, "Failed realloc: %s.", errno);
        return false;
    }
    buf->buf = realloced;
    buf->capa = capa;

    return true;
}

void iobuf_reset(struct iobuf *buf)
{
    free_safer(buf->buf);
    buf->pos  = 0;
    buf->capa = 0;
}

bool iobuf_append(struct iobuf *buf, const char *data, const size_t len)
{
    log_debug("  buf_pos=%zu, len=%zu, capa=%zu", buf->pos, len, buf->capa);
    if ((buf->pos + len > buf->capa) && !iobuf_grow(buf, len))
        return false;
    log_debug("  buf_pos=%zu, len=%zu, capa=%zu", buf->pos, len, buf->capa);

    memcpy(buf->buf + buf->pos, data, len);
    buf->pos += len;

    char *bufx = log_fmt_hex(LOG_DEBUG, (unsigned char*)buf->buf, len);
    log_debug("iobuf=%s", bufx);
    free_safer(bufx);

    return true;

}
