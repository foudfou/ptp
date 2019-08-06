/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#ifndef SAFER_H
#define SAFER_H

#include <stdbool.h>
#include <stdlib.h>

#define free_safer(p) do {                      \
        free(p);                                \
        p = NULL;                               \
    } while(0)

#define fclose_safer(fid) do {                  \
        fclose(fid);                            \
        fid = NULL;                             \
    } while(0)

/**
 * Prevent compilation if assertion is false or not a compile time constant.
 * Thanks to http://www.jaggersoft.com/pubs/CVu11_3.html
 */
#define assert_compilation(isTrue) \
    void assert_compilation(char x[1 - (!(isTrue))])

bool strcpy_safer(char *dst, const char *src, size_t size);

#endif /* SAFER_H */
