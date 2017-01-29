#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils/bit.h"
#include "log.h"
#include "server.h"

#define MAX_CONN 256
#define SECOND 1000
#define SERVER_BUFLEN 10


static int server_sock_setnonblock(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        log_perror("Failed get fcntl: %s.", errno);
        return errno;
    }
    if (fcntl(sock, F_SETFL, flags |= O_NONBLOCK) == -1) {
        log_perror("Failed set fcntl: %s.", errno);
        return errno;
    }

    return 0;
}

static int server_sock_setopts(int sock, const int family) {
    const int so_false = 0, so_true = 1;
    if ((setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&so_true,
                   sizeof(so_true)) < 0) ||
        (family == AF_INET6 &&
         setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&so_false,
                   sizeof(so_true)) < 0)) {
        log_perror("Failed setsockopt: %s.", errno);
        return errno;
    }

    return 0;
}

/**
 * Returns the listening socket fd, or -1 if failure.
 */
static int server_init(const char bind_addr[], const char bind_port[])
{
    int listendfd = -1;

    // getaddrinfo for host
    struct addrinfo hints, *addrs;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;
    if (getaddrinfo(bind_addr, bind_port, &hints, &addrs)) {
        log_perror("Failed getaddrinfo: %s.", errno);
        return -1;
    }

    // socket and bind
    struct addrinfo *it;
    for (it = addrs; it; it=it->ai_next) {
        listendfd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (listendfd == -1)
            continue;

        if (server_sock_setopts(listendfd, it->ai_family))
            return -1;

        if (server_sock_setnonblock(listendfd))
            return -1;

        if (bind(listendfd, it->ai_addr, it->ai_addrlen) == 0)
            break;

        close(listendfd);
    }

    if (!it) {
        log_perror("Failed connect: %s.", errno);
        return -1;
    }

    freeaddrinfo(addrs);

    if (listen(listendfd, 32)) {
        log_perror("Failed listen: %s.", errno);
        return -1;
    }

    return listendfd;
}

static int server_shutdown(int sock)
{
    log_info("Stopping server.");
    return close(sock);
}

/**
 * Drain all incoming connections
 */
static bool server_accept_all(const int listenfd, struct pollfd fds[], int *nfds)
{
    struct sockaddr_storage client_addr = {0};
    socklen_t client_addr_len = sizeof(client_addr);
    int conn;
    do {
        conn = accept(listenfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (conn < 0) {
            if (errno != EWOULDBLOCK) {
                log_perror("Failed server_conn_accept: %s.", errno);
                return false;
            }
            break;
        }
        log_debug("Incoming connection...");

        char host[NI_MAXHOST], service[NI_MAXSERV];
        int rv = getnameinfo((struct sockaddr *) &client_addr, client_addr_len,
                             host, NI_MAXHOST, service, NI_MAXSERV,
                             NI_NUMERICHOST | NI_NUMERICSERV);
        if (rv) {
            log_error("Failed to getnameinfo: %s.", gai_strerror(rv));
            continue;
        }

        log_info("Accepted connection from peer [%s]:%s.", host, service);
        fds[*nfds].fd = conn;
        fds[*nfds].events = POLLIN;
        (*nfds)++;
    } while (conn != -1);

    return true;
}

static int server_conn_close(int client)
{
    log_info("Closing connection with peer [%%s]:%%s."); // TODO:
    return close(client);
}

bool server_handle_data(const struct pollfd fdp[])
{
    bool conn_close = false;

    char buf[SERVER_BUFLEN];
    memset(buf, 0, SERVER_BUFLEN);
    ssize_t slen = recv(fdp->fd, buf, SERVER_BUFLEN, 0);
    if (slen < 0) {
        if (errno != EWOULDBLOCK) {
            log_perror("Failed recv: %s", errno);
            conn_close = true;
        }
        goto end;
    }

    if (slen == 0) {
        log_info("Client [%%s]:%%s closed connection."); // TODO: , host, service
        conn_close = true;
        goto end;
    }
    log_debug("Received %d bytes.", slen);

    /* Echo back */
    int resp = send(fdp->fd, buf, slen, MSG_NOSIGNAL);
    if (resp < 0) {
        if (errno == EPIPE)
            log_info("Client [%%s]:%%s disconnected."); // TODO: , host, service
        else
            log_perror("Failed send: %s.", errno);
        conn_close = true;
        goto end;
    }

  end:
    return conn_close;
}

/**
 * Main event loop
 *
 * Initially inspired from
 * https://www.ibm.com/support/knowledgecenter/en/ssw_i5_54/rzab6/poll.htm
 */
void server_run(const struct config *conf)
{
    int sock = server_init(conf->bind_addr, conf->bind_port);
    if (sock < 0) {
        log_fatal("Could not start server. Aborting.");
        return;
    }
    log_info("Server started and listening on [%s]:%s.",
             conf->bind_addr, conf->bind_port);

    struct pollfd fds[MAX_CONN] = {0};
    int nfds = 1;
    fds[0].fd = sock;
    fds[0].events = POLLIN; // FIXME: |POLLPRI ?

    bool server_end = false, compress_array = false;
    do {
        log_debug("Waiting to poll...");
        if (poll(fds, nfds, -1) < 0) {
            if (errno == EINTR)
                continue;
            else {
                log_perror("Failed poll: %s", errno);
                break;
            }
        }

        for (int i = 0; i < nfds; i++) {
            if (fds[i].revents == 0)
                continue;

            if (!BITS_CHK(fds[i].revents, POLLIN)) {
                log_error("Unexpected revents: %#x", fds[i].revents);
                server_end = true;
                break;
            }

            // FIXME: consistency with server_handle_data
            if (BITS_CHK(fds[i].revents, (POLLERR | POLLHUP)))
                log_warning("Peer closed connection. fd=%d", i);

            if (fds[i].fd == sock) {
                if (!server_accept_all(sock, fds, &nfds)) {
                    server_end = true;
                    break;
                }
            }
            else {
                log_debug("Data available on fd %d.", fds[i].fd);

                bool conn_close = server_handle_data(&fds[i]);
                if (conn_close) {
                    server_conn_close(fds[i].fd);
                    fds[i].fd = -1;
                    compress_array = true;
                }

            }  /* End readable data */
        } /* End loop poll fds */

        /* FIXME: can't we make this smarter ? */
        if (compress_array) {
            compress_array = false;
            for (int i = 0; i < nfds; i++) {
                if (fds[i].fd == -1) {
                    for(int j = i; j < nfds; j++)
                        fds[j].fd = fds[j+1].fd;
                    nfds--;
                }
            }
        }

    } while (server_end == false);

    server_shutdown(sock);
}
