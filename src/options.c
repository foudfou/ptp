#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "utils/safe.h"
#include "options.h"
#include "config.h"

const struct config CONFIG_DEFAULT = {
    .bind_addr = "::",
    .bind_port = "22000",
    .logtype   = LOG_TYPE_STDOUT,
    .loglevel  = LOG_UPTO(LOG_INFO),
};

static void usage(void)
{
    printf("Usage: %s [parameters]\n", PACKAGE_NAME);
    printf("\nParameters:\n"
           " -a, --addr=[addr]       Set bind address (ip4 or ip6)\n"
           " -p, --port=[port]       Set bind port\n"
           " -l, --log=[level]       Set log level (debug..critical)\n"
           " -o, --output=[file]     Set log output file\n"
           " -s, --syslog            Use syslog\n"
           " -h, --help              Print help and usage\n"
           " -v, --version           Print version of the server\n");
}

/**
 * Returns 0 for help, 1 on error, 2 on success.
 */
int options_parse(struct config *conf, const int argc, char *const argv[])
{
    int c;
    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"addr",    required_argument, 0, 'a'},
            {"port",    required_argument, 0, 'p'},
            {"log",     required_argument, 0, 'l'},
            {"output",  required_argument, 0, 'o'},
            {"syslog",  no_argument,       0, 's'},
            {"help",    no_argument,       0, 'h'},
            {"version", no_argument,       0, 'v'},
            {0}
        };

        c = getopt_long(argc, argv, "a:p:l:o:shv",
                        long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'a':
            if (!safe_strcpy(conf->bind_addr, optarg, NI_MAXHOST)) {
                fprintf(stderr, "wrong value for --listen\n");
                return 1;
            }
            break;

        case 'p':
            if (!safe_strcpy(conf->bind_port, optarg, NI_MAXSERV)) {
                fprintf(stderr, "wrong value for --port\n");
                return 1;
            }
            break;

        case 'l': {
            int sevmask = 0;
            for (int i = 0; log_severities[i].id; i++) {
                if (strcmp(log_severities[i].name, optarg) == 0) {
                    sevmask = log_severities[i].id;
                    break;
                }
            }
            if (!sevmask) {
                fprintf(stderr, "wrong value for --log\n");
                return 1;
            }
            conf->loglevel = sevmask;
            break;
        }

        case 'o':
            // TODO:
            break;

        case 's':
            conf->logtype = LOG_TYPE_SYSLOG;
            break;

        case 'h':
            usage();
            return 0;
            break;

        case 'v':
            printf("%s\n", PACKAGE_VERSION);
            return 0;
            break;

        case '?':
            fprintf(stderr, "???\n");
            break;

        default:
            usage();
            return 1;
        }
    }

    if (optind < argc) {
        printf("ignored arguments: ");
        while (optind < argc)
            printf("%s ", argv[optind++]);
        printf("\n");
    }

    return 2;
}
