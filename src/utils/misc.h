/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef MISC_H
#define MISC_H

#include <stdlib.h>

/**
 * Good practice to use sthg_NONE as first enum element so we can later just
 * test (!lookup_by...).
 */
typedef struct {
    int         id;
    const char *name;
} lookup_entry;

const char *lookup_by_id(const lookup_entry names[], const int id);
int lookup_by_name(const lookup_entry names[], const char name[], size_t slen);

#endif /* MISC_H */
