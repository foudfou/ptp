/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <getopt.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils/safer.h"
#include "options.h"
#include "file.h"
#include "config.h"

const struct config CONFIG_DEFAULT = {
    .conf_dir  = "~/.config/ptp", // FIXME intended for dht.state
    .bind_addr = "::",
    .bind_port = "22000",
    .log_type  = LOG_TYPE_STDOUT,
    .log_level = LOG_UPTO(LOG_INFO),
    .max_peers = 256,
};

static void usage(void)
{
    printf("Usage: %s [parameters]\n", PACKAGE_NAME);
    printf("\nParameters:\n"
           " -a, --addr=[addr]       Set bind address (ip4 or ip6)\n"
           " -c, --config=[path]     Set the config directory path\n"
           " -l, --log=[level]       Set log level (debug..critical)\n"
           " -m, --max-peers=[max]   Set maximum number of peers\n"
           " -o, --output=[file]     Set log output file\n"
           " -p, --port=[port]       Set bind port\n"
           " -s, --syslog            Use syslog\n"
           " -h, --help              Print help and usage\n"
           " -v, --version           Print version of the server\n");
}

bool init_conf_dir(struct config *conf) {
    char abspath[PATH_MAX] = "\0";
    if (!resolve_path(conf->conf_dir, abspath)) {
        fprintf(stderr, "Cannot resolve path %s.\n", conf->conf_dir);
        return false;
    }

    if (access(abspath, R_OK | W_OK) == -1) {
        if (errno == ENOENT) {
            printf("Init: creating configuration directory: %s\n", abspath);
            if (mkdir(abspath, 0700) == -1) {
                perror(abspath);
                return false;
            }
        }
        else {
            perror(abspath);
            return false;
        }
    }

    if (!strcpy_safer(conf->conf_dir, abspath, PATH_MAX)) {
        fprintf(stderr, "Configuration directory initialization failed.\n");
        return false;
    }
    return true;
}


bool init_config(struct config *conf) {
    if (!init_conf_dir(conf)) {
        return false;
    }

    return true;
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
            {"addr",       required_argument, 0, 'a'},
            {"config",     required_argument, 0, 'c'},
            {"log",        required_argument, 0, 'l'},
            {"max-peers",  required_argument, 0, 'm'},
            {"output",     required_argument, 0, 'o'},
            {"port",       required_argument, 0, 'p'},
            {"syslog",     no_argument,       0, 's'},
            {"help",       no_argument,       0, 'h'},
            {"version",    no_argument,       0, 'v'},
            {0}
        };

        c = getopt_long(argc, argv, "a:c:l:m:o:p:shv",
                        long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'a':
            if (!strcpy_safer(conf->bind_addr, optarg, NI_MAXHOST)) {
                fprintf(stderr, "Wrong value for --listen.\n");
                return 1;
            }
            break;

        case 'c':
            if (!strcpy_safer(conf->conf_dir, optarg, PATH_MAX)) {
                fprintf(stderr, "Wrong value for --config.\n");
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
                fprintf(stderr, "Wrong value for --log.\n");
                return 1;
            }
            conf->log_level = sevmask;
            break;
        }

        case 'm': {
            struct rlimit nofile = {0};
            if (getrlimit(RLIMIT_NOFILE, &nofile) == -1) {
                perror("getrlimit");
                return 1;
            }

            errno = 0;
            long val = strtol(optarg, NULL, 10);
            if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
                || (errno != 0 && val == 0)
                || (val < 1 || val > (long)nofile.rlim_cur)) {
                fprintf(stderr, "Wrong value for --max-peers."
                        " Should be in [1, %zd].\n", nofile.rlim_cur);
                return 1;
            }
            conf->max_peers = (size_t)val;
            break;
        }

        case 'o':
            // TODO:
            break;

        case 'p':
            if (!strcpy_safer(conf->bind_port, optarg, NI_MAXSERV)) {
                fprintf(stderr, "Wrong value for --port.\n");
                return 1;
            }
            break;

        case 's':
            conf->log_type = LOG_TYPE_SYSLOG;
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
        fprintf(stderr, "Ignored arguments: ");
        while (optind < argc)
            fprintf(stderr, "%s ", argv[optind++]);
        fprintf(stderr, "\n");
    }

    if (!init_config(conf)) {
        fprintf(stderr, "Configuration initialization failed. Aborting\n");
        return 1;
    }

    return 2;
}
