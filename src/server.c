/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils/bits.h"
#include "utils/cont.h"
#include "utils/list.h"
#include "utils/safe.h"
#include "config.h"
#include "log.h"
#include "signals.h"
#include "server.h"

// FIXME: low for testing purpose.
#define SERVER_BUFLEN 10
#define POLL_EVENTS POLLIN|POLLPRI

struct peer {
    struct list_item item;
    int              fd;
    char             host[NI_MAXHOST];
    char             service[NI_MAXSERV];
};


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

static bool server_shutdown(int sock)
{
    if (close(sock) == -1) {
        log_perror("Failed close: %s.", errno);
        return false;
    }
    log_info("Stopping server.");
    return true;
}

static struct peer*
peer_register(struct list_item *peers, int conn,
              struct sockaddr_storage *addr, socklen_t *addr_len)
{
    char host[NI_MAXHOST], service[NI_MAXSERV];
    int rv = getnameinfo((struct sockaddr *) addr, *addr_len,
                         host, NI_MAXHOST, service, NI_MAXSERV,
                         NI_NUMERICHOST | NI_NUMERICSERV);
    if (rv) {
        log_error("Failed to getnameinfo: %s.", gai_strerror(rv));
        return NULL;
    }

    struct peer *peer = malloc(sizeof(struct peer));
    peer->fd = conn;
    strcpy(peer->host, host);
    strcpy(peer->service, service);
    list_init(&(peer->item));
    list_append(peers, &(peer->item));
    log_debug("Peer [%s]:%s registered (fd=%d).", host, service, conn);

    return peer;
}

static struct peer*
peer_find_by_fd(struct list_item *peers, const int fd)
{
    struct peer *p;
    struct list_item * it = peers;
    list_for(it, peers) {
        p = cont(it, struct peer, item);
        if (p->fd == fd)
            break;
    }

    if (it == peers) {
        log_warning("Peer not found fd=%d.", fd);
        return NULL;
    }

    return p;
}

static void peer_unregister(struct peer *peer)
{
    log_debug("Unregistering peer [%s]:%s.", peer->host, peer->service);
    list_delete(&peer->item);
    safe_free(peer);
}

/**
 * Drain all incoming connections
 */
static bool peer_accept_all(const int listenfd, struct list_item *peers)
{
    struct sockaddr_storage peer_addr = {0};
    socklen_t peer_addr_len = sizeof(peer_addr);
    int conn;
    int fail = 0;
    do {
        // TODO: refuse conn if > MAX_PEERS

        conn = accept(listenfd, (struct sockaddr *)&peer_addr, &peer_addr_len);
        if (conn < 0) {
            if (errno != EWOULDBLOCK) {
                log_perror("Failed server_conn_accept: %s.", errno);
                return false;
            }
            break;
        }
        log_debug("Incoming connection...");

        struct peer *p = peer_register(peers, conn, &peer_addr, &peer_addr_len);
        if (!p) {
            fail++;
            continue;
        }
        log_info("Accepted connection from peer [%s]:%s.", p->host, p->service);

    } while (conn != -1);

    return fail ? false : true;
}

static bool peer_conn_close(struct peer *peer)
{
    bool ret = true;
    log_info("Closing connection with peer [%s]:%s.", peer->host, peer->service);
    if (close(peer->fd) == -1) {
        log_perror("Failed closed for peer: %s.", errno);
        ret = false;
    }
    peer_unregister(peer);
    return ret;
}

static bool peer_conn_close_all(struct list_item *peers)
{
    int fail = 0;

    struct peer *p;
    struct list_item * it = peers;
    while (it->prev != peers) {
        p = cont(it->next, struct peer, item);
        if (!peer_conn_close(p))
            fail++;
    }

    return fail ? false : true;
}

static bool server_handle_data(const struct peer *peer)
{
    bool conn_close = false;

    char buf[SERVER_BUFLEN];
    memset(buf, 0, SERVER_BUFLEN);
    ssize_t slen = recv(peer->fd, buf, SERVER_BUFLEN, 0);
    if (slen < 0) {
        if (errno != EWOULDBLOCK) {
            log_perror("Failed recv: %s", errno);
            conn_close = true;
        }
        goto end;
    }

    if (slen == 0) {
        log_info("Peer [%s]:%s closed connection.", peer->host, peer->service);
        conn_close = true;
        goto end;
    }
    log_debug("Received %d bytes.", slen);

    /* Echo back */
    int resp = send(peer->fd, buf, slen, MSG_NOSIGNAL);
    if (resp < 0) {
        if (errno == EPIPE)
            log_info("Peer [%s]:%s disconnected while sending.",
                     peer->host, peer->service);
        else
            log_perror("Failed send: %s.", errno);
        conn_close = true;
        goto end;
    }

  end:
    return conn_close;
}

static int pollfds_update(struct pollfd fds[], struct list_item *peer_list)
{
    struct list_item * it = peer_list;
    int npeer = 1;
    list_for(it, peer_list) {
        struct peer *p = cont(it, struct peer, item);
        fds[npeer].fd = p->fd;
        fds[npeer].events = POLL_EVENTS;
        npeer++;
    }
    return npeer;
}

/**
 * Main event loop
 *
 * poll(2) is portable and should be sufficient as we don't expect to handle
 * thousands of peer connections.
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

    struct pollfd fds[conf->max_peers];
    memset(fds, 0, sizeof(fds));
    int nfds = 1;
    fds[0].fd = sock;
    fds[0].events = POLL_EVENTS;
    struct list_item peer_list = LIST_ITEM_INIT(peer_list);

    bool server_end = false;
    do {
        if (BITS_CHK(sig_events, EV_SIGINT)) {
            BITS_CLR(sig_events, EV_SIGINT);
            log_info("Caught SIGINT. Shutting down.");
            server_end = true;
            break;
        }

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

            if (!BITS_CHK(fds[i].revents, POLL_EVENTS)) {
                log_error("Unexpected revents: %#x", fds[i].revents);
                server_end = true;
                break;
            }

            if (fds[i].fd == sock) {
                if (peer_accept_all(sock, &peer_list))
                    continue;
                else {
                    server_end = true;
                    break;
                }
            }

            log_debug("Data available on fd %d.", fds[i].fd);

            struct peer *p = peer_find_by_fd(&peer_list, fds[i].fd);
            if (!p) {
                log_fatal("Could not handle data for unregister peer fd=%d.", fds[i].fd);
                server_end = true;
                break;
            }

            bool peer_disconnect = server_handle_data(p);
            if (peer_disconnect && !peer_conn_close(p)) {
                log_fatal("Could not close connection of peer fd=%d.", fds[i].fd);
                server_end = true;
                break;
            }

        } /* End loop poll fds */

        nfds = pollfds_update(fds, &peer_list);

    } while (server_end == false);

    peer_conn_close_all(&peer_list);
    server_shutdown(sock);
}
