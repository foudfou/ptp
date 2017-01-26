#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "options.h"
#include "log.h"
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

    int sock = server_start(conf.bind_addr, conf.bind_port);
    if (sock < 0) {
        log_fatal("Could not start server. Aborting.");
        goto cleanup_log;
    }
    log_info("Server started and listening on [%s]:%s.", conf.bind_addr, conf.bind_port);

    int client = server_conn_accept(sock);

    char buf[SERVER_BUFLEN];
    for (;;) {
        memset(buf, 0, SERVER_BUFLEN);
        ssize_t slen = recv(client, buf, SERVER_BUFLEN, 0);

        char data[SERVER_BUFLEN+21+1] = {0};
        snprintf(data, SERVER_BUFLEN+21, "Echoing back - (%03ld) %s", slen, buf);
        data[SERVER_BUFLEN+21] = '\0';
        log_debug("%s", data);

        int resp = send(client, buf, slen + 1, MSG_NOSIGNAL);
        if (resp == -1 && errno == EPIPE) {
            log_info("Client [%%s]:%%s disconnected."); // TODO: , host, service
            break;
        }
    }

    server_conn_close(client);

    server_stop(sock);
  cleanup_log:
    log_shutdown(conf.logtype);

    return 0;
}
