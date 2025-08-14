/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include <limits.h>
#include "log.h"
#include "utils/cont.h"
#include "utils/time.h"
#include "utils/safer.h"
#include "timers.h"

bool set_timeout(struct list_item *timers, long long delay, bool once,
                 struct event *evt)
{
    struct timer *timer = malloc(sizeof(struct timer));
    if (!timer) {
        log_perror(LOG_ERR, "Failed malloc: %s.", errno);
        return false;
    }
    *timer = (struct timer){
        .name={0}, .once=once, .delay=delay,
        .event=evt, .self=timer
    };
    strcpy_safer(timer->name, evt->name, EVENT_NAME_MAX);
    timer_init(timers, timer, 0);
    return true;
}

/**
 * @time optional: will be set to current time if 0.
 */
bool timer_init(struct list_item *timers, struct timer *t, long long time)
{
    if (time == 0 && (time = now_millis()) < 0)
        return false;

    t->expire = time + t->delay;
    list_append(timers, &t->item);
    log_debug("timer '%s' inited, expire=%lld", t->name, t->expire);

    return true;
}

bool timers_free_all(struct list_item *timers)
{
    log_debug("Freeing remaining timers and events.");
    while (!list_is_empty(timers)) {
        struct timer *t = cont(timers->prev, struct timer, item);
        list_delete(timers->prev);
        if (t->event && t->event->self) {
            free_event(t->event->self);
        }
        if (t->self) {
            free(t->self);
        }
    }
    return true;
}

int timers_get_soonest(struct list_item *timers)
{
    long long tick = now_millis();
    if (tick < 0)
        return -1;
    log_debug("tick=%lld", tick);

    long long soonest = LLONG_MAX;

    struct list_item * it = timers;
    list_for(it, timers) {
        const struct timer *t = cont(it, struct timer, item);
        if (!t) {
            log_error("Undefined container in list.");
            return -2;
        }

        if (t->expire - tick < soonest) {
            soonest = t->expire - tick;
        }
    }

    if (soonest < 0) // some timers already expired
        soonest = 0;
    else if (soonest == LLONG_MAX)
        soonest = -1;

    return soonest;
}

bool timers_apply(struct list_item *timers, event_queue *evq)
{
    long long tack = now_millis();
    if (tack < 0)
        return false;
    log_debug("tack=%lld", tack);

    unsigned int errors = 0;
    struct list_item * it = timers;
    list_for(it, timers) {
        struct timer *t = cont(it, struct timer, item);
        if (!t) {
            log_error("Undefined container in list.");
            return false;
        }

        int missed = 0;
        while (t->expire <= tack) {
            log_debug("timer '%s' triggered (missed=%ux)", t->name, missed);
            if (!event_queue_put(evq, t->event)) {
                log_error("Enqueue event '%s' failed.", t->event->name);
                errors++;
            }
            /* FIXME handle `catch_up` flag: defaults to false, tells if we
               need to fire the timer handler as many times as we missed,
               in case the event loop lasted more than a timer period. */
            if (t->once) {
                it = it->prev; // deleting inside for_list
                list_delete(&t->item);
                if (t->self) {
                    free(t->self);
                }
                break;
            }
            else {
                t->expire += t->delay;
                missed++;
            }
        }

    } // End for_list

    return !errors;
}
