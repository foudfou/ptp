/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef PTP_STRING_H
#define PTP_STRING_H

#include <stdbool.h>

// http://blog.liw.fi/posts/strncpy/
bool safe_strcpy(char *dst, const char *src, size_t size);

#endif /* PTP_STRING_H */
