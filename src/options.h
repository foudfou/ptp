/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef OPTIONS_H
#define OPTIONS_H

#include <netdb.h>
#include "log.h"

struct config {
    char       bind_addr[NI_MAXHOST];
    char       bind_port[NI_MAXSERV];
    logtype_t  logtype;
    int        loglevel;
};

extern const struct config CONFIG_DEFAULT;

int options_parse(struct config *conf, const int argc, char *const argv[]);

#endif /* OPTIONS_H */
