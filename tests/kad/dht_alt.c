/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "net/kad/dht.c"      // testing static functions

/* These are for testing dht operations like lookups in a "real" (tiny) space. */
int main ()
{
    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_DEBUG)));
    struct kad_dht *dht = dht_init();

    dht->self_id.bytes[0] = 0x00;
    char *id = log_fmt_hex(LOG_DEBUG, dht->self_id.bytes, KAD_GUID_SPACE_IN_BYTES);
    log_debug("self_id reset (id=%s)", id);
    free_safer(id);

    struct kad_node_info info = {.host = "1.1.1.1", .service = "22"};
    kad_guid_set(&info.id, (unsigned char []){0x01}); // don't initialize id literaly

    id = log_fmt_hex(LOG_DEBUG, info.id.bytes, KAD_GUID_SPACE_IN_BYTES);
    log_debug("info (id=%s)", id);
    free_safer(id);

    assert(dht_insert(dht, &info));

    dht_terminate(dht);
    log_shutdown(LOG_TYPE_STDOUT);


    return 0;
}
