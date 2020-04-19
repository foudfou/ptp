/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include <limits.h>
#include "log.h"
#include "utils/time.h"

static clockid_t clockid = CLOCK_MONOTONIC;

static inline long long millis_from_timespec(struct timespec t) {
    return (t).tv_sec * 1000LL + (t).tv_nsec / 1e6;
}

bool clock_res_is_millis()
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

static bool now(struct timespec *tspec)
{
    if (clock_gettime(clockid, tspec) < 0) {
        log_perror(LOG_ERR, "Failed clock_gettime: %s", errno);
        return false;
    }
    return true;
}

long long now_millis()
{
    struct timespec tspec = {0};
    if (!now(&tspec))
        return -1;
    return millis_from_timespec(tspec);
}

bool now_sec(time_t *t)
{
    struct timespec tspec = {0};
    if (!now(&tspec))
        return false;
    *t = tspec.tv_sec;
    return true;
}
