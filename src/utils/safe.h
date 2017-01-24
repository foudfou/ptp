/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef SAFE_H
#define SAFE_H

#include <stdbool.h>
#include <stdlib.h>

#define safe_free(p) do {                       \
        free(p);                                \
        p = NULL;                               \
    } while(0)

#define safe_fclose(fid) do {                   \
        fclose(fid);                            \
        fid = NULL;                             \
    } while(0)

/**
 * Prevent compilation if assertion is false or not a compile time constant.
 * Thanks to http://www.jaggersoft.com/pubs/CVu11_3.html
 */
#define assert_compilation(isTrue) \
    void assert_compilation(char x[1 - (!(isTrue))])

// http://blog.liw.fi/posts/strncpy/
bool safe_strcpy(char *dst, const char *src, size_t size);

#endif /* SAFE_H */
