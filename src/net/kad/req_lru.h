/* Copyright (c) 2020 Foudil Brétel.  All rights reserved. */
#ifndef REQ_LRU_H
#define REQ_LRU_H

#include <stdint.h>
#include "net/kad/rpc.h"
#include "utils/hash.h"
#include "utils/helpers.h"

/**
 * We want to keep track of sent queries. For ex. during node lookup: « Nodes
 * that fail to respond quickly are removed from consideration until and unless
 * they do respond. »
 *
 * We need fast access (=> hash by tx_id), fixed-sized for safety, and
 * expiration (=> FIFO linked-list). This is very similar to a LRU cache except
 * we don't refresh on lookup.
 */
#define REQ_LRU_CAPACITY 1024

#define HREQ_LRU_SIZE 0xff

/* We MUST provide our own hash function. See hash.h.
   Here is djb2 reported by Dan Bernstein in comp.lang.c. */
static inline uint32_t hreq_lru_hash(const kad_rpc_msg_tx_id ary)
{
    unsigned long hash = 5381;
    for (size_t i = 0; i < KAD_RPC_MSG_TX_ID_LEN; i++)
        hash = ((hash << 5) + hash) + ary.bytes[i];
    return hash;
}

/* We MUST provide our own key comparison function. See hash.h. */
static inline int hreq_lru_cmp(const kad_rpc_msg_tx_id ary1, const kad_rpc_msg_tx_id ary2)
{
    return memcmp(&ary1.bytes, &ary2.bytes, KAD_RPC_MSG_TX_ID_LEN);
}

HASH_GENERATE(hreq_lru, kad_rpc_query, hitem, msg.tx_id, kad_rpc_msg_tx_id, HREQ_LRU_SIZE)

struct req_lru {
    size_t           len;
    struct list_item litems;
    struct list_item hitems[HREQ_LRU_SIZE];
};

static inline void req_lru_init(struct req_lru *lru) {
    list_init(&lru->litems);
    hash_init(lru->hitems, HREQ_LRU_SIZE);
}

static inline void req_lru_terminate(struct req_lru *lru) {
    struct list_item *items = &lru->litems;
    list_free_all(items, struct kad_rpc_query, litem);
}

/**
 * Exclusive insert.
 *
 * Returns false on duplicated. Sets @evicted to evicted item.
 */
static inline bool
req_lru_put(struct req_lru *lru, struct kad_rpc_query *q,
            struct kad_rpc_query **evicted) {
    const struct kad_rpc_query *dup = hreq_lru_get(lru->hitems, q->msg.tx_id);
    if (dup)
        return false;

    if (lru->len >= REQ_LRU_CAPACITY) {
        struct list_item *last = lru->litems.prev;
        list_delete(last);
        struct kad_rpc_query *evict = cont(last, struct kad_rpc_query, litem);
        hash_delete(&evict->hitem);
        if (evicted)
            *evicted = evict;
    }

    list_insert(&lru->litems, &q->litem);
    hreq_lru_insert(lru->hitems, q->msg.tx_id, &q->hitem);
    lru->len++;

    return true;
}

static inline struct kad_rpc_query *
req_lru_get(struct req_lru *lru, const kad_rpc_msg_tx_id id) {
    return hreq_lru_get(lru->hitems, id);
}

/**
 * Delete query with tx_id @id from queries and return it in @out.
 */
static inline bool
req_lru_delete(struct req_lru *lru, const kad_rpc_msg_tx_id id, struct kad_rpc_query **out) {
    *out = req_lru_get(lru, id);
    if (!*out)
        return false;
    hash_delete(&(*out)->hitem);
    list_delete(&(*out)->litem);
    lru->len--;
    return true;
}


#endif /* REQ_LRU_H */
