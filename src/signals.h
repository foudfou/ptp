/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef SIGNALS_H
#define SIGNALS_H

#include <signal.h>
#include <stdbool.h>

#define EV_SIGINT   1 << 0
#define EV_SIGHUP   1 << 1
#define EV_SIGQUIT  1 << 2
#define EV_SIGTERM  1 << 3
#define EV_SIGUSR1  1 << 8
#define EV_SIGUSR2  1 << 9
#define EV_SIGALRM  1 << 10

#define EV_NEXT_CONT 0
#define EV_NEXT_FIN  1

extern volatile sig_atomic_t sig_events;

bool sig_install();


#endif /* SIGNALS_H */
