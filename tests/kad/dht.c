/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "net/kad/dht.c"      // testing static functions
#include "utils/safer.h"

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


    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_CRIT)));
    struct kad_dht *dht = dht_init();

    kad_guid nid1 = {.b = {[KAD_GUID_BYTE_SPACE-1]=0x0}};
    assert(dht_update(dht, &nid1) == 1);
    assert(dht_can_insert(dht, &nid1) == NULL);
    assert(dht_insert(dht, &nid1, "192.168.168.1", "12354"));
    /* dht_get_node() is not exposed. So we're not supposed to do bad things
       like freeing a node, or assert(!n1) after it's been dht_delete'd. */
    struct kad_node *n1 = dht_get(dht, &nid1, NULL);
    assert(n1);

    assert(dht_delete(dht, &nid1));
    n1 = dht_get(dht, &nid1, NULL);
    assert(!n1);

    dht_terminate(dht);
    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
