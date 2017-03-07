/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include <string.h>
#include "utils/misc.h"

enum smth {
    SMTH_NONE,
    SMTH_ONE,
    SMTH_TWO,
    SMTH_THREE,
};

static const lookup_entry smth_names[] = {
    { SMTH_ONE,   "one" },
    { SMTH_TWO,   "two" },
    { SMTH_THREE, "three" },
    { 0,          NULL }
};


int main(void)
{
    assert(strcmp(lookup_by_id(smth_names, SMTH_ONE), "one") == 0);
    assert(lookup_by_id(smth_names, 42) == NULL);

    assert(lookup_by_name(smth_names, "one", 5) == SMTH_ONE);
    assert(lookup_by_name(smth_names, "none", 5) == 0);
    assert(lookup_by_name(smth_names, "none", 5) == SMTH_NONE);


    return 0;
}
