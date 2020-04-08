/* Copyright (c) 2020 Foudil Brétel.  All rights reserved. */
#include <stdint.h>
#include "net/kad/rpc.h"
#include "utils/hash.h"

/**
 * We want to keep track of sent queries. For ex. « Nodes that fail to respond
 * quickly are removed from consideration until and unless they do respond. »
 *
 * We need fast access (hash by tx_id), fixed-sized and expiration
 * (linked-list). This is very similar to a LRU cache except we don't refresh
 * on lookup.
 */
#define QUERIES_CAPACITY 1024

#define HQUERIES_SIZE 0xff

/* We MUST provide our own hash function. See hash.h.
   Here is djb2 reported by Dan Bernstein in comp.lang.c. */
static inline uint32_t hqueries_hash(const kad_rpc_msg_tx_id ary)
{
    unsigned long hash = 5381;
    for (size_t i = 0; i < KAD_RPC_MSG_TX_ID_LEN; i++)
        hash = ((hash << 5) + hash) + ary.bytes[i];
    return hash;
}

/* We MUST provide our own key comparison function. See hash.h. */
static inline int hqueries_cmp(const kad_rpc_msg_tx_id ary1, const kad_rpc_msg_tx_id ary2)
{
    return memcmp(&ary1.bytes, &ary2.bytes, KAD_RPC_MSG_TX_ID_LEN);
}

HASH_GENERATE(hqueries, kad_rpc_query, hitem, msg.tx_id, kad_rpc_msg_tx_id, HQUERIES_SIZE)

struct queries {
    size_t           len;
    struct list_item lqueries;
    struct list_item hqueries[HQUERIES_SIZE];
};

void queries_init(struct queries *lru) {
    list_init(&lru->lqueries);
    hash_init(lru->hqueries, HQUERIES_SIZE);
}

/**
 * Returns evicted item or NULL.
 */
struct kad_rpc_query *
queries_put(struct queries *lru, struct kad_rpc_query *q) {
    struct kad_rpc_query *rv = NULL;

    if (lru->len >= QUERIES_CAPACITY) {
        struct list_item *last = lru->lqueries.prev;
        list_delete(last);
        struct kad_rpc_query *evicted = cont(last, struct kad_rpc_query, litem);
        hash_delete(&evicted->hitem);
        rv = evicted;
    }

    list_insert(&lru->lqueries, &q->litem);
    hqueries_insert(lru->hqueries, q->msg.tx_id, &q->hitem);
    lru->len++;

    return rv;
}

/**
 * Delete query with tx_id `id` from queries and return it.
 */
struct kad_rpc_query *
queries_delete(struct queries *lru, const kad_rpc_msg_tx_id id) {
    struct kad_rpc_query *rv = NULL;
    rv = hqueries_get(lru->hqueries, id);
    hash_delete(&rv->hitem);
    list_delete(&rv->litem);
    lru->len--;
    return rv;
}
