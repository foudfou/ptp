/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include "safer.h"

// http://blog.liw.fi/posts/strncpy/
bool strcpy_safer(char *dst, const char *src, size_t size)
{
    if (strlen(src) + 1 > size)
        return false;
    strcpy(dst, src);
    return true;
}
