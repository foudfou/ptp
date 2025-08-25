/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef IOBUF_H
#define IOBUF_H

#include "utils/growable.h"

GROWABLE_GENERATE(iobuf, char, 32, 2)
GROWABLE_GENERATE_APPEND(iobuf, char)

#endif /* IOBUF_H */
