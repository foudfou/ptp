#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "options.h"

#define BUFLEN 10

int server_applysockopts(const int sock, const int family) {
    const int so_false = 0, so_true = 1;
    if ((setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&so_true,
                   sizeof(so_true)) < 0) ||
        (family == AF_INET6 &&
         setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&so_false,
                   sizeof(so_true)) < 0)) {
        perror("setsockopt");
        return errno;
    }
    return 0;
}

/**
 * Returns the listening socket fd, or -1 if failure.
 */
int server_start(const char bind_addr[], const char bind_port[])
{
    int listendfd = -1;

    // getaddrinfo for host
    struct addrinfo hints, *addrs;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;
    if (getaddrinfo(bind_addr, bind_port, &hints, &addrs)) {
        perror ("getaddrinfo");
        return -1;
    }

    // socket and bind
    struct addrinfo *it;
    for (it = addrs; it; it=it->ai_next) {
        listendfd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (listendfd == -1)
            continue;

        if (server_applysockopts(listendfd, it->ai_family))
            return -1;

        if (bind(listendfd, it->ai_addr, it->ai_addrlen) == 0)
            break;

        close(listendfd);
    }

    if (!it) {
        perror("Could not connect");
        return -1;
    }

    freeaddrinfo(addrs);

    if (listen(listendfd, 10)) {
        perror("listen");
        return -1;
    }

    return listendfd;
}

int server_accept(const int listenfd)
{
    struct sockaddr_storage client_addr = {0};
    socklen_t client_addr_len = sizeof(client_addr);
    int conn =
        accept(listenfd, (struct sockaddr *)&client_addr, &client_addr_len);

    char host[NI_MAXHOST], service[NI_MAXSERV];
    int rv = getnameinfo((struct sockaddr *) &client_addr, client_addr_len,
                         host, NI_MAXHOST, service, NI_MAXSERV,
                         NI_NUMERICHOST | NI_NUMERICSERV);
    if (rv == 0)
        printf("[%s]:%s connected\n", host, service);
    else
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(rv));

    return conn;
}


int main(int argc, char *argv[])
{
    struct config conf = CONFIG_DEFAULT;

    int rv = options_parse(&conf, argc, argv);
    if (rv < 2)
        return rv;

    int sock = server_start(conf.bind_addr, conf.bind_port);
    if (sock < 0) {
        perror("server_start");
        return 1;
    }

    int client = server_accept(sock);

    char buf[BUFLEN];
    for (;;) {
        memset(buf, 0, BUFLEN);
        ssize_t slen = recv(client, buf, BUFLEN, 0);

        char data[BUFLEN+21+1] = {0};
        snprintf(data, BUFLEN+21, "Echoing back - (%03ld) %s", slen, buf);
        data[BUFLEN+21] = '\0';
        printf("%s", data);

        int resp = send(client, buf, slen + 1, MSG_NOSIGNAL);
        if (resp == -1 && errno == EPIPE) {
            /* printf("\n[%s]:%s disconnected\n", host, service); */
            break;
        }
    }

    close(client);
    close(sock);

    return 0;
}
