/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "proto/kad.c"      // testing static functions

int main ()
{
    size_t bkt_idx = kad_node_id_to_bucket(
        (kad_guid){.dd = 0x0}, (kad_guid){.dd = 0x1}, KAD_GUID_SPACE);
    assert(bkt_idx == 0);
    bkt_idx = kad_node_id_to_bucket(
        (kad_guid){.dd = 0x1}, (kad_guid){.dd = 0x1}, KAD_GUID_SPACE);
    assert(bkt_idx == 0);
    bkt_idx = kad_node_id_to_bucket(
        (kad_guid){.dd = 0x8}, (kad_guid){.dd = 0x1}, KAD_GUID_SPACE);
    assert(bkt_idx == 3);


    return 0;
}
