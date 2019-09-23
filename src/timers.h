/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
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
 * See also https://nodejs.org/fa/docs/guides/event-loop-timers-and-nexttick/
 */
#include <stdbool.h>
#include "utils/list.h"

#define TIMER_NAME_MAX 64

typedef bool (*timerHandlerFunc)(int hi);

struct timer {
    struct list_item item;
    char             name[TIMER_NAME_MAX];
    long long        ms;
    long long        expire;
    bool             catch_up;
    timerHandlerFunc cb;
};

bool timers_clock_res_is_millis();
/** Before the event loop. */
bool timers_init(struct list_item *timers);
/** Right before poll() to calculate its `timeout` parameter. */
int timers_get_soonest(struct list_item *timers);
/** After poll() has returned. */
bool timers_apply(struct list_item *timers);

#endif /* TIMERS_H */
