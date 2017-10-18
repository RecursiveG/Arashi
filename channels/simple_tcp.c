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

void simple_tcp_init(channel_simple_tcp_t *tcp) {
    memset(tcp, 0, sizeof(*tcp));
    tcp->listen_fd = -1;
    tcp->channel_fd = -1;
}

void simple_tcp_clean(channel_simple_tcp_t *tcp) {
    simple_tcp_deafen(tcp);
    simple_tcp_disconnect(tcp);
    simple_tcp_init(tcp);
}

int simple_tcp_listen(channel_simple_tcp_t *tcp, const char *addr, const char *service, uev_ctx_t *ctx) {
    struct addrinfo *listen_addr;
    int fd = -1;

    if (0 != getaddrinfo(addr, service, NULL, &listen_addr)) goto err;
    if (0 > (fd = socket(AF_INET, SOCK_STREAM, 0))) goto err;
    if (0 > bind(fd, listen_addr->ai_addr, listen_addr->ai_addrlen)) goto err;
    if (0 > listen(fd, SOMAXCONN)) goto err;
    if (0 > set_fd_non_block(fd)) goto err;

    freeaddrinfo(listen_addr);
    tcp->listen_fd = fd;
    tcp->listen_watcher = malloc(sizeof(uev_t));
    uev_io_init(ctx, tcp->listen_watcher, simple_tcp_incoming_conn_ev, tcp, fd, UEV_READ);
    return fd;
err:
    if (fd >= 0) close(fd);
    return -1;
}

void simple_tcp_deafen(channel_simple_tcp_t *tcp) {
    if (tcp->listen_watcher != NULL) {
        uev_io_stop(tcp->listen_watcher);
        free(tcp->listen_watcher);
        tcp->listen_watcher = NULL;
    }
    if (tcp->listen_fd >= 0) {
        close(tcp->listen_fd);
        tcp->listen_fd = -1;
    }
}

int simple_tcp_connect(channel_simple_tcp_t *tcp, const char *addr, const char *service, uev_ctx_t *ctx) {
    struct addrinfo *peer_addr;
    int fd = -1;

    if (0 != getaddrinfo(addr, service, NULL, &peer_addr)) goto err;
    if (0 > (fd = socket(AF_INET, SOCK_STREAM, 0))) goto err;
    if (0 > connect(fd, peer_addr->ai_addr, peer_addr->ai_addrlen)) goto err;
    if (0 > set_fd_non_block(fd)) goto err;

    freeaddrinfo(peer_addr);
    tcp->channel_fd = fd;
    tcp->recv_watcher = malloc(sizeof(uev_t));
    uev_io_init(ctx, tcp->recv_watcher, simple_tcp_incoming_data_ev, tcp, fd, UEV_READ);
    log_info("Connected to remote host %s:%s", addr, service);
    return fd;
err:
    if (fd >= 0) close(fd);
    return -1;
}

void simple_tcp_disconnect(channel_simple_tcp_t *tcp) {
    if (tcp->recv_watcher != NULL) {
        uev_io_stop(tcp->recv_watcher);
        free(tcp->recv_watcher);
        tcp->recv_watcher = NULL;
    }
    if (tcp->channel_fd >= 0) {
        close(tcp->channel_fd);
        tcp->channel_fd = -1;
    }
    memset(&tcp->recv_buf, 0, sizeof(tcp->recv_buf));
    log_info("simple_tcp disconnected");
}

void simple_tcp_incoming_conn_ev(uev_t *w, void *arg, int events) {
    channel_simple_tcp_t *tcp = arg;
    int new_fd = accept(w->fd, NULL, NULL);
    if (new_fd < 0) {
        log_error("accept() fail: %s", strerror(errno));
        return;
    }

    log_info("Incoming connection ...");
    if (tcp->channel_fd >= 0) {
        close(new_fd);
        log_error("Connection refused: Cannot handle multiple connections");
        return;
    }

    tcp->channel_fd = new_fd;
    tcp->recv_watcher = malloc(sizeof(uev_t));
    uev_io_init(w->ctx, tcp->recv_watcher, simple_tcp_incoming_data_ev, tcp, new_fd, UEV_READ);
    log_info("Connection established");
}

void simple_tcp_incoming_data_ev(uev_t *w, void *arg, int events) {
    channel_simple_tcp_t *tcp = arg;
    simple_tcp_buf *buf = &tcp->recv_buf;

    if (buf->pkt_size == 0) { // transmission header not ready
        ssize_t recv_len = recv(w->fd, (uint8_t*)&buf->pkt.header + buf->processed_size, SIMPLE_TCP_HEADER_LEN - buf->processed_size, 0);
        if (recv_len == 0) { // connection closed
            simple_tcp_disconnect(tcp);
            if (tcp->listen_fd <= 0) uev_exit(w->ctx);
        } else if (recv_len == -1) { // connection error
            if (errno == EAGAIN) return;
            log_error("recv() fail: %s", strerror(errno));
            simple_tcp_disconnect(tcp);
            if (tcp->listen_fd <= 0) uev_exit(w->ctx);
        } else { // (part of) header read
            buf->processed_size += recv_len;
            if (buf->processed_size == SIMPLE_TCP_HEADER_LEN) {
                buf->pkt_size = ntohl(buf->pkt.header.pkt_size_be);
                buf->processed_size = 0;
                log_info("incoming packet of size=%d", buf->pkt_size);
            }
        }
    } else { // header ready but body incomplete
        ssize_t recv_len = recv(w->fd, (uint8_t*)&buf->pkt.body + buf->processed_size, buf->pkt_size - buf->processed_size, 0);
        if (recv_len == 0) { // connection closed
            simple_tcp_disconnect(tcp);
            if (tcp->listen_fd <= 0) uev_exit(w->ctx);
        } else if (recv_len == -1) { // connection error
            if (errno == EAGAIN) return;
            log_error("recv() fail: %s", strerror(errno));
            simple_tcp_disconnect(tcp);
            if (tcp->listen_fd <= 0) uev_exit(w->ctx);
        } else { // (part of) header read
            buf->processed_size += recv_len;
            if (buf->processed_size == buf->pkt_size) {
                tcp->packet_ready(tcp, buf->pkt.body, buf->pkt_size);
                buf->pkt_size = 0;
                buf->processed_size = 0;
            }
        }
    }
}

static int socket_write_sync(int fd, const uint8_t buf[], size_t size) {
    size_t rem = size;
    size_t pos = 0;
    while(rem > 0) {
        ssize_t l = send(fd, buf + pos, rem, 0);
        if (l < 0) {
            if (errno == EAGAIN) continue;
            return -1;
        }
        rem -= l;
        pos += l;
    }
    return 0;
}

int simple_tcp_write(channel_simple_tcp_t *tcp, const uint8_t buf[], size_t size) {
    if (tcp->channel_fd < 0) {
        log_warn("tcp channel not ready");
        return 0;
    }

    simple_tcp_pkt_header header;
    header.pkt_size_be = htonl((uint32_t) size);
    int success = socket_write_sync(tcp->channel_fd, (void*)&header, sizeof(header));
    if (success < 0) return -1;
    success = socket_write_sync(tcp->channel_fd, buf, size);
    if (success < 0) return -1;
    return 0;
}
