/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
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

bool timers_init(struct list_item *timers)
{
    struct timespec tspec = {0};
    if (clock_gettime(clockid, &tspec) < 0) {
        log_perror(LOG_ERR, "Failed clock_gettime: %s", errno);
        return false;
    }
    long long tick_init = millis_from_timespec(tspec);
    log_debug("tick_init=%lld", tick_init);

    struct list_item * it = timers;
    list_for(it, timers) {
        struct timer *t = cont(it, struct timer, item);
        if (!t) {
            log_error("Undefined container in list.");
            return false;
        }
        t->expire = tick_init + t->ms;
        log_debug("timer '%s' inited, expire=%lld", t->name, t->expire);
    }

    return true;
}

int timers_get_soonest(struct list_item *timers)
{
    struct timespec tspec = {0};
    if (clock_gettime(clockid, &tspec) < 0) {
        log_perror(LOG_ERR, "Failed clock_gettime: %s", errno);
        return -1;
    }
    long long tick = millis_from_timespec(tspec);
    log_debug("tick=%lld", tick);

    long long soonest = LLONG_MAX;

    struct list_item * it = timers;
    list_for(it, timers) {
        struct timer *t = cont(it, struct timer, item);
        if (!t) {
            log_error("Undefined container in list.");
            return -2;
        }

        if (t->expire - tick < soonest)
            soonest = t->expire - tick;
    }

    return soonest == LLONG_MAX ? -1 : soonest;
}

bool timers_apply(struct list_item *timers, event_queue *evq)
{
    struct timespec tspec = {0};
    if (clock_gettime(clockid, &tspec) < 0) {
        log_perror(LOG_ERR, "Failed clock_gettime: %s", errno);
        return false;
    }
    long long tack = millis_from_timespec(tspec);
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
            log_debug("timer '%s' (missed=%ux)", t->name, missed);
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
                t->expire += t->ms;
                missed++;
            }
        }

    } // End for_list

    return !errors;
}
