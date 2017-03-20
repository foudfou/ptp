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

    bkt_idx = kad_bucket_hash(
        &(kad_guid){.b = {[0]=0xff, [1]=0x0}},
        &(kad_guid){.b = {[0]=0x7f, [1]=0x0}});
    assert(bkt_idx == KAD_GUID_SPACE-1);

    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_CRIT)));
    struct kad_dht *dht = dht_init();

    struct kad_node_info info = { .id = {.b = {[KAD_GUID_BYTE_SPACE-1]=0x0}},
                                  .host = "1.1.1.1", .service = "22" };
    struct kad_node_info old = {0};
    assert(dht_update(dht, &info) == 1);
    assert(dht_can_insert(dht, &info.id, &old));
    assert(dht_insert(dht, &info));
    /* dht_get_node() is not exposed. So we're not supposed to do bad things
       like freeing a node, or assert(!n1) after it's been dht_delete'd. */
    struct kad_node *n1 = dht_get(dht, &info.id, NULL);
    assert(n1);

    assert(dht_delete(dht, &info.id));
    n1 = dht_get(dht, &info.id, NULL);
    assert(!n1);

    // bucket full
    struct kad_node_info opp;
    memcpy(opp.id.b, dht->self_id.b, KAD_GUID_BYTE_SPACE);
    opp.id.b[0] ^= 0x80;
    assert(KAD_K_CONST <= 0xff);  // don't want to overflow next
    for (int i = 0; i < KAD_K_CONST; ++i) {
        assert(dht_can_insert(dht, &opp.id, &old));
        assert(dht_insert(dht, &opp));
        opp.id.b[KAD_GUID_BYTE_SPACE-1] += 1;
    }
    assert(!dht_can_insert(dht, &opp.id, &old));

    dht_terminate(dht);
    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}