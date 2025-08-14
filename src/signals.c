/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <stdbool.h>
#include "utils/bits.h"
#include "log.h"
#include "signals.h"

volatile sig_atomic_t sig_events = 0;

static void sig_handler(int signo)
{
    switch (signo) {
    case SIGALRM:
        BITS_SET(sig_events, EV_SIGALRM);
        break;
    case SIGUSR1:
        BITS_SET(sig_events, EV_SIGUSR1);
        break;
    case SIGUSR2:
        BITS_SET(sig_events, EV_SIGUSR2);
        break;
    case SIGTERM: /* ^\ */
    case SIGQUIT: /* ^\ */
    case SIGHUP:
    case SIGINT:  /* ^C */
        BITS_SET(sig_events, EV_SIGINT);
        break;
    default:
        log_error("Caught unhandled signal %d\n", signo);
    }
}

bool sig_install()
{
    int sig[][2] = {
        {SIGALRM, SA_RESTART},
        {SIGUSR1, SA_RESTART},
        {SIGUSR2, SA_RESTART},
        {SIGINT,  SA_RESTART},
        {SIGHUP,  SA_RESTART},
        {SIGQUIT, SA_RESTART},
        {SIGTERM, SA_RESTART}
    };
    int len = sizeof(sig) / sizeof(int) / 2;

    struct sigaction sa = {0};
    sa.sa_handler = sig_handler;
    for (int i = 0; i < len; i++) {
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = sig[i][1];
        if (sigaction(sig[i][0], &sa, NULL) != 0) {
            log_perror(LOG_ERR, "Failed sigaction: %s", errno);
            return false;
        }
    }

    return true;
}
