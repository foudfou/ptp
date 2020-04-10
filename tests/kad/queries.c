/* Copyright (c) 2020 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "net/kad/rpc.c"
#include "timers.c"
#include "utils/bits.h"
#include "net/kad/queries.h"

bool query_init(struct kad_rpc_query *q) {
    list_init(&q->litem);
    list_init(&q->hitem);
    kad_rpc_generate_tx_id(&q->msg.tx_id);
    return (q->created = now_millis()) != -1;
}

#include <stdio.h>
int main()
{
    struct queries queries = {0};
    queries_init(&queries);
    assert(queries.len == 0 && list_count(&queries.lqueries) == 0);

    struct kad_rpc_query q0 = {0};
    assert(query_init(&q0));
    assert(queries_put(&queries, &q0) == NULL);
    assert(queries.len == 1 && list_count(&queries.lqueries) == 1);
    assert(queries_get(&queries, q0.msg.tx_id) == &q0);

    struct kad_rpc_query *q = NULL;
    kad_rpc_msg_tx_id tx_id = q0.msg.tx_id;
    BITS_TGL(tx_id.bytes[0], 1);
    assert(!queries_delete(&queries, tx_id, &q));
    assert(queries.len == 1 && list_count(&queries.lqueries) == 1);

    assert(queries_delete(&queries, q0.msg.tx_id, &q));
    assert(q == &q0);
    assert(queries.len == 0 && list_count(&queries.lqueries) == 0);
    assert(queries_get(&queries, q0.msg.tx_id) == NULL);

    for (int i = 0; i < QUERIES_CAPACITY; i++) {
        q = calloc(1, sizeof(struct kad_rpc_query));
        assert(q);
        assert(query_init(q));
        assert(queries_put(&queries, q) == NULL);
    }
    assert(queries.len == QUERIES_CAPACITY && list_count(&queries.lqueries) == QUERIES_CAPACITY);

    struct kad_rpc_query *oldest = cont(queries.lqueries.prev, struct kad_rpc_query, litem);
    q = calloc(1, sizeof(struct kad_rpc_query));
    assert(q);
    assert(query_init(q));
    struct kad_rpc_query *evicted = queries_put(&queries, q);
    assert(evicted);
    assert(evicted == oldest);
    free(evicted);

    while (!list_is_empty(&queries.lqueries)) {
        q = cont(queries.lqueries.prev, struct kad_rpc_query, litem);
        assert(queries_delete(&queries, q->msg.tx_id, &q));
        free(q);
    }
    assert(list_count(&queries.lqueries) == 0);


    return 0;
}
