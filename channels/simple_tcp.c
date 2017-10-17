#include "simple_tcp.h"
#include "../external/log.h"

#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <malloc.h>
#include <errno.h>
#include <string.h>

static int set_fd_non_block(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;

    int err = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (err == -1) return -1;

    return 0;
}

void simple_tcp_conn_init(channel_simple_tcp_conn_t *conn) {
    conn ->fd = -1;
    conn->packet_ready = NULL;
    conn->recd_size=0;
    conn->recv_size=0;
}

void simple_tcp_init(channel_simple_tcp_t *tcp) {
    tcp->listen_fd = -1;
    tcp->opened_channels = 0;
    tcp->default_packet_ready = NULL;
}

// return listen fd, or -1 on failure
int simple_tcp_listen(channel_simple_tcp_t *tcp, const char *addr, const char *service) {
    struct addrinfo *listen_addr;
    int err;
    err = getaddrinfo(addr, service, NULL, &listen_addr);
    if (err != 0) return -1;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    tcp->listen_fd = fd;
    if (fd < 0) return -1;

    if (set_fd_non_block(fd) < 0) return -1;

    err = bind(fd, listen_addr->ai_addr, listen_addr->ai_addrlen);
    if (err < 0) return -1;

    err = listen(fd, SOMAXCONN);
    if (err < 0) return -1;

    freeaddrinfo(listen_addr);
    return fd;
}

// return new connection id, or -1 on failure
int simple_tcp_connect(channel_simple_tcp_t *tcp, const char *addr, const char *service) {
    struct addrinfo *peer_addr;
    int err;
    err = getaddrinfo(addr, service, NULL, &peer_addr);
    if (err != 0) return -1;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    err = connect(fd, peer_addr->ai_addr, peer_addr->ai_addrlen);
    if (err < 0) return -1;

    if (set_fd_non_block(fd) < 0) return -1;

    freeaddrinfo(peer_addr);
    channel_simple_tcp_conn_t *conn = malloc(sizeof(*conn));
    simple_tcp_conn_init(conn);
    conn->fd = fd;
    conn->packet_ready = tcp -> default_packet_ready;

    tcp->channels[tcp->opened_channels++] = conn;
    return tcp->opened_channels-1;
}

void simple_tcp_incoming_data(uev_t *w, void *arg, int events) {
    channel_simple_tcp_conn_t *conn = arg;

    if (conn->recv_size > 0) {
        ssize_t recv_len = recv(w->fd, conn->recv_buf.pkt+conn->recd_size, conn->recv_size - conn->recd_size, 0);
        if (recv_len < 0 && errno != EAGAIN) {
            log_error("recv() fail: %s", strerror(errno));
            uev_io_stop(w);
            return;
        }

        conn->recd_size += recv_len;
        if (conn->recd_size == conn->recv_size) {
            if (conn->packet_ready == NULL)
                log_warn("packet_ready is NULL");
            else
                conn->packet_ready(conn, conn->recv_buf.pkt, conn->recv_size);
            conn->recv_size = 0;
            conn->recd_size = 0;
        }
    } else { // HEADER NOT READY
        ssize_t recv_len = recv(w->fd, &conn->recv_buf+conn->recd_size, SIMPLE_TCP_HEADER_LEN - conn->recd_size, 0);
        if (recv_len < 0 && errno != EAGAIN) {
            log_error("recv() fail: %s", strerror(errno));
            uev_io_stop(w);
            return;
        }

        conn->recd_size += recv_len;
        if (conn->recd_size == SIMPLE_TCP_HEADER_LEN) {
            conn->recv_size = ntohs(conn->recv_buf.pkt_size_be);
            conn->recd_size = 0;
            log_info("incoming packet of size=%d", conn->recv_size);
        }
    }
}

void simple_tcp_incoming_conn(uev_t *w, void *arg, int events) {
    channel_simple_tcp_t *tcp = arg;
    int new_fd = accept(w->fd, NULL, NULL);
    if (new_fd < 0) {
        log_error("accept() fail: %s", strerror(errno));
        uev_io_stop(w);
        return;
    }
    log_info("Incoming connection ...");
    if (tcp ->opened_channels >= 1) {
        close(new_fd);
        log_warn("Connection closed due to existing connection.");
        return;
    }

    channel_simple_tcp_conn_t *conn = malloc(sizeof(*conn));
    simple_tcp_conn_init(conn);
    conn->fd = new_fd;
    conn->packet_ready = tcp->default_packet_ready;
    tcp->channels[tcp->opened_channels++] = conn;

    uev_t *wn = malloc(sizeof(*wn));
    uev_io_init(w->ctx, wn, simple_tcp_incoming_data, conn, new_fd, UEV_READ);
}