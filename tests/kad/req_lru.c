/* Copyright (c) 2020 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "net/kad/rpc.c"
#include "timers.c"
#include "utils/bits.h"
#include "net/kad/req_lru.h"

bool query_init(struct kad_rpc_query *q) {
    list_init(&q->litem);
    list_init(&q->hitem);
    kad_rpc_generate_tx_id(&q->msg.tx_id);
    return (q->created = now_millis()) != -1;
}

#include <stdio.h>
int main()
{
    struct req_lru reqs_out = {0};
    req_lru_init(&reqs_out);
    assert(reqs_out.len == 0 && list_count(&reqs_out.litems) == 0);

    struct kad_rpc_query q0 = {0};
    assert(query_init(&q0));
    assert(req_lru_put(&reqs_out, &q0, NULL));
    assert(reqs_out.len == 1 && list_count(&reqs_out.litems) == 1);
    assert(req_lru_get(&reqs_out, q0.msg.tx_id) == &q0);

    struct kad_rpc_query *q = NULL;
    kad_rpc_msg_tx_id tx_id = q0.msg.tx_id;
    BITS_TGL(tx_id.bytes[0], 1);
    assert(!req_lru_delete(&reqs_out, tx_id, &q));
    assert(reqs_out.len == 1 && list_count(&reqs_out.litems) == 1);

    assert(req_lru_delete(&reqs_out, q0.msg.tx_id, &q));
    assert(q == &q0);
    assert(reqs_out.len == 0 && list_count(&reqs_out.litems) == 0);
    assert(req_lru_get(&reqs_out, q0.msg.tx_id) == NULL);

    for (int i = 0; i < REQ_LRU_CAPACITY; i++) {
        q = calloc(1, sizeof(struct kad_rpc_query));
        assert(q);
        assert(query_init(q));
        if (i % 255)
            q->msg.tx_id.bytes[0] = i / 255;
        q->msg.tx_id.bytes[KAD_RPC_MSG_TX_ID_LEN-1] = i;
        assert(req_lru_put(&reqs_out, q, NULL));
    }
    assert(reqs_out.len == REQ_LRU_CAPACITY && list_count(&reqs_out.litems) == REQ_LRU_CAPACITY);

    struct kad_rpc_query *oldest = cont(reqs_out.litems.prev, struct kad_rpc_query, litem);
    q = calloc(1, sizeof(struct kad_rpc_query));
    assert(q);
    assert(query_init(q));
    struct kad_rpc_query *evicted = NULL;
    assert(req_lru_put(&reqs_out, q, &evicted));
    assert(evicted);
    assert(evicted == oldest);
    free(evicted);

    while (!list_is_empty(&reqs_out.litems)) {
        q = cont(reqs_out.litems.prev, struct kad_rpc_query, litem);
        assert(req_lru_delete(&reqs_out, q->msg.tx_id, &q));
        free(q);
    }
    assert(list_count(&reqs_out.litems) == 0);


    return 0;
}
