/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "utils/byte_array.h"

BYTE_ARRAY_GENERATE(some_id, 4)

int main ()
{
    some_id id1;
    BYTE_ARRAY_INIT(some_id, id1);

    some_id_set(&id1, (unsigned char*)"aaa");
    assert(id1.is_set);
    assert(some_id_eq(&id1, &(some_id){.bytes = "aaa", .is_set = true}));


    return 0;
}
