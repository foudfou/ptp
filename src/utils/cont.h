/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#ifndef CONT_H
#define CONT_H

#include <stddef.h>

/**
 * Computes the pointer to the structure of type @type that contains a @member
 * variable pointed by @ptr.
 */
#define cont(ptr, type, member) \
    (ptr ? ((type*) (((char*) ptr) - offsetof(type, member))) : NULL)

#endif /* CONT_H */
