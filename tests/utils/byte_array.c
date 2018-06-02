/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "utils/byte_array.h"

#define SOME_ID_LEN_IN_BYTES 4
BYTE_ARRAY_GENERATE(some_id, SOME_ID_LEN_IN_BYTES)

int main ()
{
    some_id id1;
    BYTE_ARRAY_INIT(some_id, id1);
    assert(some_id_eq(&id1, &(some_id){.bytes = {0}, .is_set = false}));

    some_id_set(&id1, (unsigned char*)"aaa");
    assert(id1.is_set);
    assert(some_id_eq(&id1, &(some_id){.bytes = "aaa", .is_set = true}));

    some_id_reset(&id1);
    assert(some_id_eq(&id1, &(some_id){.bytes = {0}, .is_set = false}));


    some_id id2;
    BYTE_ARRAY_INIT(some_id, id2);

    assert(some_id_setbit(&id2, 0) &&
           some_id_eq(&id2, &(some_id){.bytes = {[SOME_ID_LEN_IN_BYTES-1]=0x1}}));
    assert(some_id_setbit(&id2, 1) &&
           some_id_eq(&id2, &(some_id){.bytes = {[SOME_ID_LEN_IN_BYTES-1]=0x3}}));
    assert(some_id_setbit(&id2, 7) &&
           some_id_eq(&id2, &(some_id){.bytes = {[SOME_ID_LEN_IN_BYTES-1]=0x83}}));
    assert(!some_id_setbit(&id2, SOME_ID_LEN_IN_BYTES*8));
    assert(!some_id_setbit(&id2, SOME_ID_LEN_IN_BYTES*8 + 7));

    return 0;
}
