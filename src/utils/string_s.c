/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <stddef.h>
#include <string.h>
#include <sys/types.h>

#include "defs.h"
#include "string_s.h"

int safe_strcpy(char *dst, const char *src, size_t size)
{
    if (strlen(src) + 1 > size)
        return PTP_ENOSPC;
    strcpy(dst, src);
    return PTP_SUCC;
}
