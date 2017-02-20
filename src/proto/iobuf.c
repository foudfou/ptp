/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <errno.h>
#include "log.h"
#include "proto/iobuf.h"

bool iobuf_grow(struct iobuf *buf)
{
    size_t nchk = buf->nchunks + 1;
    buf->buf = realloc(buf->buf, nchk * PROTO_IO_CHUNK);
    if (!buf->buf) {
        log_perror("Failed realloc: %s.", errno);
        return false;
    }
    buf->nchunks = nchk;
    return true;
}

void iobuf_shrink(struct iobuf *buf)
{
    free(buf);
    buf->pos     = 0;
    buf->nchunks = 0;
}
