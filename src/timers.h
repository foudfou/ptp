/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#ifndef TIMERS_H
#define TIMERS_H

/**
 * Timers baked into the server's event loop.
 *
 * To handle timers in our event loop, we seem to have basically 2 options:
 * make timers write to some fd (in a self-pipe manner) or use the `timeout`
 * parameter of poll(). Since timers use signals and we favor to avoid them,
 * we'll go with the second approach.
 * Thx https://stackoverflow.com/a/20204081/421846!
 *
 * Also timerfd_*() would be a nice option but are linux-specific
 * unfortunately.
 *
 * See also https://nodejs.org/en/docs/guides/event-loop-timers-and-nexttick/
 */
#include <stdbool.h>
#include "events.h"
#include "log.h"
#include "options.h"
#include "utils/list.h"

#define TIMER_NAME_MAX 64

/**
 * Timers may be periodic or once-only.
 *
 * Initialize with timer_init().
 */
struct timer {
    struct list_item   item;
    char               name[TIMER_NAME_MAX];
    /* To compute `expire` for periodic timers. */
    long long          delay;   // in ms,
    /* To compute when to trigger event (timeout). */
    long long          expire;  // timestamp for expiry
    bool               catch_up;
    bool               once;
    /* Address to self when allocated. `once` timers are expected to be
       allocated. Used in timers_apply to free. The variable holding the
       pointer to the allocated memory might be gone out of scope when we free,
       so we have no reliable way to null it ourselves. It's thus best to stick
       to the convention not to hold any pointer to the allocated memory
       outside this struct. */
    struct timer      *self;
    struct event      *event;
};

bool timers_clock_res_is_millis();
long long now_millis();
bool timer_init(struct list_item *timers, struct timer *t, long long time);
/** Right before poll() to calculate its `timeout` parameter. */
int timers_get_soonest(struct list_item *timers);
/** After poll() has returned. */
bool timers_apply(struct list_item *timers, event_queue *evq);

#endif /* TIMERS_H */
