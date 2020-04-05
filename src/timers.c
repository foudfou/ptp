/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include <limits.h>
#include <time.h>
#include "utils/cont.h"
#include "timers.h"

static clockid_t clockid = CLOCK_MONOTONIC;

static inline long long millis_from_timespec(struct timespec t) {
    return (t).tv_sec * 1000LL + (t).tv_nsec / 1e6;
}

bool timers_clock_res_is_millis()
{
    struct timespec ts = {0};
    if (clock_getres(clockid, &ts) < 0) {
        log_perror(LOG_ERR, "Failed clock_getres: %s", errno);
        return false;
    }
    if (ts.tv_nsec > 1e6) {
        return false;
    }
    return true;
}

long long now_millis()
{
    struct timespec tspec = {0};
    if (clock_gettime(clockid, &tspec) < 0) {
        log_perror(LOG_ERR, "Failed clock_gettime: %s", errno);
        return -1;
    }
    return millis_from_timespec(tspec);
}

/**
 * `now` optional: will be set to current time if 0.
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

int timers_get_soonest(struct list_item *timers)
{
    long long tick = now_millis();
    if (tick < 0)
        return -1;
    log_debug("tick=%lld", tick);

    long long soonest = LLONG_MAX;

    struct list_item * it = timers;
    list_for(it, timers) {
        struct timer *t = cont(it, struct timer, item);
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
