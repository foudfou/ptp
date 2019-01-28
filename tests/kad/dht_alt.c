/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "net/kad/dht.c"      // testing static functions

/* These are for testing dht operations like lookups in a "real" (tiny) space. */
int main ()
{
    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_DEBUG)));
    struct kad_dht *dht = dht_init();


    dht->self_id.bytes[0] = 0xa0;
    char *id = log_fmt_hex(LOG_DEBUG, dht->self_id.bytes, KAD_GUID_SPACE_IN_BYTES);
    log_debug("self_id reset (id=%s)", id);
    free_safer(id);

    struct kad_node_info info = {.host = "1.1.1.1", .service = "22"};
    kad_guid_set(&info.id, (unsigned char[]){0x10}); // don't initialize id literaly

    id = log_fmt_hex(LOG_DEBUG, info.id.bytes, KAD_GUID_SPACE_IN_BYTES);
    log_debug("info (id=%s, host=%s)", id, info.host);
    free_safer(id);

    assert(dht_insert(dht, &info));


    strcpy(info.host, "1.1.2.0");
    kad_guid_set(&info.id, (unsigned char[]){0xb0});
    assert(dht_insert(dht, &info));
    strcpy(info.host, "1.1.2.1");
    kad_guid_set(&info.id, (unsigned char[]){0x80});
    assert(dht_insert(dht, &info));
    strcpy(info.host, "1.1.2.2");
    kad_guid_set(&info.id, (unsigned char[]){0xd0});
    assert(dht_insert(dht, &info));
    strcpy(info.host, "1.1.2.3");
    kad_guid_set(&info.id, (unsigned char[]){0x70});
    assert(dht_insert(dht, &info));

    kad_guid target;
    kad_guid_set(&target, (unsigned char[]){0x90}); // 0b1001
    struct kad_node_info nodes[KAD_K_CONST];
    size_t added = dht_find_closest(dht, &target, nodes);
    log_debug("____added=%zu", added);
    assert(added == 5);

    kad_guid_set(&target, (unsigned char[]){0xc0}); // 0b1100
    added = dht_find_closest(dht, &target, nodes);
    log_debug("____added=%zu", added);

    kad_guid_set(&target, (unsigned char[]){0x03}); // 0b0011
    added = dht_find_closest(dht, &target, nodes);
    log_debug("____added=%zu", added);

    // FIXME: test the order buckets are visited by checking the `nodes` array.

    dht_terminate(dht);
    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
