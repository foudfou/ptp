/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef IOBUF_H
#define IOBUF_H

#include <stdbool.h>
#include <stdlib.h>

#define PROTO_IO_CHUNK 512

/* TODO: this is probably the skeleton of a de-/serialization framework. See
   http://stackoverflow.com/a/6002598/421846 */
struct iobuf {
    char     *buf;
    size_t    pos;
    unsigned  nchunks;
};

bool iobuf_grow(struct iobuf *buf);
void iobuf_shrink(struct iobuf *buf);

#endif /* IOBUF_H */
