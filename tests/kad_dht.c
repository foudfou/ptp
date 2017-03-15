/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "net/kad/dht.c"      // testing static functions

int main ()
{
    size_t bkt_idx = kad_bucket_hash(
        &(kad_guid){.b = {[KAD_GUID_BYTE_SPACE-1]=0x0}},
        &(kad_guid){.b = {[KAD_GUID_BYTE_SPACE-1]=0x1}});
    assert(bkt_idx == 0);
    bkt_idx = kad_bucket_hash(
        &(kad_guid){.b = {[KAD_GUID_BYTE_SPACE-1]=0x1}},
        &(kad_guid){.b = {[KAD_GUID_BYTE_SPACE-1]=0x1}});
    assert(bkt_idx == 0);
    bkt_idx = kad_bucket_hash(
        &(kad_guid){.b = {[KAD_GUID_BYTE_SPACE-1]=0x8}},
        &(kad_guid){.b = {[KAD_GUID_BYTE_SPACE-1]=0x1}});
    assert(bkt_idx == 3);

    /* TODO: test kad_node_{get,insert,delete} */


    return 0;
}
