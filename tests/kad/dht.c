/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "utils/safer.h"
#include "net/kad/dht.c"      // testing static functions

int main ()
{
    size_t bkt_idx = kad_bucket_hash(
        &(kad_guid){.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x0}},
        &(kad_guid){.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x1}});
    assert(bkt_idx == 0);
    bkt_idx = kad_bucket_hash(
        &(kad_guid){.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x1}},
        &(kad_guid){.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x1}});
    assert(bkt_idx == 0);
    bkt_idx = kad_bucket_hash(
        &(kad_guid){.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x8}},
        &(kad_guid){.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x1}});
    assert(bkt_idx == 3);

    bkt_idx = kad_bucket_hash(
        &(kad_guid){.bytes = {[0]=0xff, [1]=0x0}},
        &(kad_guid){.bytes = {[0]=0x7f, [1]=0x0}});
    assert(bkt_idx == KAD_GUID_SPACE_IN_BITS-1);

    assert(!kad_guid_eq(
        &(kad_guid){.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x0}},
        &(kad_guid){.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x1}}));
    assert(!kad_guid_eq(
        &(kad_guid){.bytes = "abcdefghij0123456789"},
        &(kad_guid){.bytes = "\x00""bcdefghij0123456789"}));
    assert(kad_guid_eq(
        &(kad_guid){.bytes = "\x00""bcdefghij0123456789"},
        &(kad_guid){.bytes = "\x00""bcdefghij0123456789"}));

    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_CRIT)));
    struct kad_dht *dht = dht_init();

    struct kad_node_info info = { .id = {.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x0}},
                                  .host = "1.1.1.1", .service = "22" };
    assert(dht_update(dht, &info) == 1);
    assert(dht_insert(dht, &info));
    /* dht_get...() is not exposed. So we're not supposed to do bad things like
       freeing a node, or assert(!n1) after it's been dht_delete'd. */
    bkt_idx = kad_bucket_hash(&dht->self_id, &info.id);
    struct kad_node *n1 = dht_get_from_list(&dht->buckets[bkt_idx], &info.id);
    assert(n1);

    assert(dht_delete(dht, &info.id));
    n1 = dht_get_from_list(&dht->buckets[bkt_idx], &info.id);
    assert(!n1);

    // insert duplicate
    info.id = dht->self_id;
    assert(!dht_insert(dht, &info));

    // bucket full
    struct kad_node_info opp;
    opp.id = dht->self_id;
    opp.id.bytes[0] ^= 0x80;
    assert(KAD_K_CONST <= 0xff);  // don't want to overflow
    for (int i = 0; i < KAD_K_CONST + 1; ++i) {
        assert(dht_insert(dht, &opp));
        opp.id.bytes[KAD_GUID_SPACE_IN_BYTES-1] += 1;
    }
    assert(!list_is_empty(&dht->replacement));

    dht_terminate(dht);
    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
