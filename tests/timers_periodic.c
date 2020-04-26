/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "kad/test_util.h"
#include "utils/time.h"
#include "timers.h"

static struct event ev1 = {"event-1", .cb=NULL, .args={{{0}}}, .fatal=false};

int main ()
{
    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_CRIT)));

    assert(clock_res_is_millis());

    event_queue evq = {0};

    struct list_item timer_list = LIST_ITEM_INIT(timer_list);
    struct timer t1 = { .name="t1", .delay=250, .event=&ev1, .item=LIST_ITEM_INIT(t1.item) };
    assert(timer_init(&timer_list, &t1, 0));

    assert(event_queue_status(&evq) == QUEUE_STATE_EMPTY);
    int timeout_prev = 30000;
    for (int i=0; i<3; ++i) {
        int timeout = timers_get_soonest(&timer_list);
        assert(timeout >= -1);
        assert(timeout < timeout_prev);
        timeout_prev = timeout;
        assert(msleep(100) == 0); // say poll(.., timeout) got trigged by fd events
        assert(timers_apply(&timer_list, &evq));
    }
    assert(event_queue_status(&evq) != QUEUE_STATE_EMPTY);


    log_shutdown(LOG_TYPE_STDOUT);

    return 0;
}
