#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include "utils/string_s.h"
#include "options.h"
#include "config.h"

static void usage(void)
{
    printf("Usage: %s [parameters]\n", PACKAGE_NAME);
    printf("\nParameters:\n"
           " -l, --listen=[addr]   Run server as a daemon\n"
           " -p, --port=[port]     Print version of the server\n"
           " -h, --help            Print help and usage\n");
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
            {"listen", required_argument, 0, 'l'},
            {"port", required_argument, 0, 'p'},
            {"help", no_argument, 0, 'h'},
            {0}
        };

        c = getopt_long(argc, argv, "l:p:h",
                        long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'l':
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

        case 'h':
            usage();
            return 0;
            break;

        case '?':
            fprintf(stderr, "???\n");
            break;

        default:
            fprintf(stderr, "dafeault\n");
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
