/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "utils/queue.h"

#define QUEUE4_BIT_LEN 2
QUEUE_GENERATE(queue4, int, QUEUE4_BIT_LEN)

int main()    {
    int elt[QUEUE_BIT_LEN(2)] = {1,2,3,4};
    queue4 q1;
    queue4_init(&q1);

    assert(q1.head == 0);
    assert(q1.tail == 0);
    assert(q1.is_full == 0);
    assert(queue4_status(&q1) == QUEUE_STATE_EMPTY);
    assert(queue4_get(&q1) == NULL);

    assert(queue4_put(&q1, &elt[0]));
    assert(queue4_status(&q1) == QUEUE_STATE_OK);
    assert(*queue4_get(&q1) == 1);
    assert(queue4_status(&q1) == QUEUE_STATE_EMPTY);

    assert(queue4_put(&q1, &elt[0]));
    assert(queue4_put(&q1, &elt[1]));
    assert(queue4_put(&q1, &elt[2]));
    assert(queue4_put(&q1, &elt[3]));

    assert(!queue4_put(&q1, &elt[0]));
    assert(queue4_status(&q1) == QUEUE_STATE_FULL);

    assert(*queue4_get(&q1) == 1);
    assert(*queue4_get(&q1) == 2);
    assert(*queue4_get(&q1) == 3);
    assert(*queue4_get(&q1) == 4);
}
