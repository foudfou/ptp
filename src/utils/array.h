/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#ifndef ARRAY_H
#define ARRAY_H

#include <stddef.h>
#define ARRAY_LEN(arr)    (sizeof(arr) / sizeof((arr)[0]))
#define ARRAY_SLEN(arr)   ((ptrdiff_t)ARRAY_SIZE(arr))
#define ARRAY_BYTES(arr)  (sizeof((arr)[0]) * ARRAY_SIZE(arr))

#endif /* ARRAY_H */
