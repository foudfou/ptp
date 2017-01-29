#include <stdio.h>
#include "log.h"
#include "options.h"
#include "server.h"

int main(int argc, char *argv[])
{
    struct config conf = CONFIG_DEFAULT;
    int rv = options_parse(&conf, argc, argv);
    if (rv < 2)
        return rv;

    if (!log_init(conf.logtype, conf.loglevel)) {
        fprintf(stderr, "Could not setup logging. Aborting.\n");
        return 1;
    }

    server_run(&conf);

    log_shutdown(conf.logtype);

    return 0;
}
