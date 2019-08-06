/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#ifndef IOBUF_H
#define IOBUF_H

#include <stdbool.h>
#include <stdlib.h>

#define IOBUF_SIZE_INITIAL 512
#define IOBUF_SIZE_FACTOR  2

struct iobuf {
    char     *buf;
    size_t    pos;
    unsigned  capa;
};

void iobuf_reset(struct iobuf *buf);
bool iobuf_append(struct iobuf *buf, const char *data, const size_t len);

#endif /* IOBUF_H */
