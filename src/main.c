/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#include <stdio.h>
#include "log.h"
#include "options.h"
#include "server.h"
#include "signals.h"

int main(int argc, char *argv[])
{
    if (!sig_install()) {
        fprintf(stderr, "Could not install signals. Aborting.\n");
        return EXIT_FAILURE;
    }

    struct config conf = CONFIG_DEFAULT;
    int rv = options_parse(&conf, argc, argv);
    if (rv < 2)
        return rv;

    if (!log_init(conf.log_type, conf.log_level)) {
        fprintf(stderr, "Could not setup logging. Aborting.\n");
        return EXIT_FAILURE;
    }

    log_info("Using config directory: %s", conf.conf_dir);

    bool ret = server_run(&conf);

    log_shutdown(conf.log_type);

    return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}
