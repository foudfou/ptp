#ifndef OPTIONS_H
#define OPTIONS_H

#include <netdb.h>

struct config {
    char bind_addr[NI_MAXHOST];
    char bind_port[NI_MAXSERV];
};
static struct config CONFIG_DEFAULT = {
    .bind_addr = "::",
    .bind_port = "22000",
};

int options_parse(struct config *conf, const int argc, char *const argv[]);

#endif /* OPTIONS_H */
