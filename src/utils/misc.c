/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <string.h>
#include "utils/misc.h"

const char *lookup_by_id(const lookup_entry names[], const int id)
{
    while (names->name != NULL && names->id != id)
        names++;
    return names->name;
}

int lookup_by_name(const lookup_entry names[], const char name[], size_t slen)
{
    while (names->name != NULL && strncmp(names->name, name, slen) != 0)
        names++;
    return names->id;
}
