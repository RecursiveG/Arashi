#include "simple_tcp.h"
#include "../external/log.h"
#include "../router.h"
#include "socks5_client.h"

#include <unistd.h>
#include <stdlib.h>
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
    tcp->router_pkt_handler = (accept_pkt_f*) simple_tcp_fw_pkt;
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
    uev_io_init(ctx, tcp->listen_watcher, simple_tcp_bw_conn_ev, tcp, fd, UEV_READ);
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
    uev_io_init(ctx, tcp->recv_watcher, simple_tcp_bw_data_ev, tcp, fd, UEV_READ);
    log_info("simple_tcp: Connected to remote host %s:%s", addr, service);
    return fd;
err:
    if (fd >= 0) close(fd);
    return -1;
}

int simple_tcp_via_socks5(channel_simple_tcp_t *tcp, socks5_client_t *socks5_client, uev_ctx_t *ctx) {
    if (0 > set_fd_non_block(socks5_client->fd)) {
        if (socks5_client->fd >= 0) close(socks5_client->fd);
        return -1;
    }
    tcp->channel_fd = socks5_client->fd;
    tcp->recv_watcher = malloc(sizeof(uev_t));
    uev_io_init(ctx, tcp->recv_watcher, simple_tcp_bw_data_ev, tcp, tcp->channel_fd, UEV_READ);
    return 0;
}

void simple_tcp_disconnect(channel_simple_tcp_t *tcp) {
    if (tcp->recv_watcher != NULL) {
        log_debug("stopping watcher");
        uev_io_stop(tcp->recv_watcher);
        free(tcp->recv_watcher);
        tcp->recv_watcher = NULL;
    }
    if (tcp->channel_fd >= 0) {
        close(tcp->channel_fd);
        tcp->channel_fd = -1;
    }
    memset(&tcp->recv_buf, 0, sizeof(tcp->recv_buf));
    log_info("simple_tcp: Disconnected");
}

void simple_tcp_bw_conn_ev(uev_t *w, void *arg, int events) {
    channel_simple_tcp_t *tcp = arg;
    int new_fd = accept(w->fd, NULL, NULL);
    if (new_fd < 0) {
        log_error("simple_tcp: accept() fail: %s", strerror(errno));
        return;
    }

    log_info("simple_tcp: Incoming connection ...");
    if (tcp->channel_fd >= 0) {
        close(new_fd);
        log_error("simple_tcp: Connection refused: Cannot handle multiple connections");
        return;
    }

    tcp->channel_fd = new_fd;
    tcp->recv_watcher = malloc(sizeof(uev_t));
    uev_io_init(w->ctx, tcp->recv_watcher, simple_tcp_bw_data_ev, tcp, new_fd, UEV_READ);
    log_info("simple_tcp: Connection established");
}

void simple_tcp_bw_data_ev(uev_t *w, void *arg, int events) {
    channel_simple_tcp_t *tcp = arg;
    simple_tcp_buf *buf = &tcp->recv_buf;
    if (buf->pkt == NULL) {
        buf->pkt = router_request_pkt();
        buf->expecting_length = sizeof(simple_tcp_pkt_header);
        buf->header_parsed = false;
    }

    void *data_buf;
    size_t expecting_len;
    ssize_t recv_len;

receive_packet:
    data_buf = buf->pkt->start + buf->pkt->size;
    expecting_len = buf->expecting_length - buf->pkt->size;
    recv_len = recv(w->fd, data_buf, expecting_len, 0);
    log_debug("simple_tcp: expecting_len = %ld, recv_len=%ld", expecting_len, recv_len);

    if (recv_len == 0) { // connection closed
        log_debug("simple_tcp: recv() return 0");
        simple_tcp_disconnect(tcp);
        if (tcp->listen_fd <= 0) uev_exit(w->ctx);
    } else if (recv_len == -1) { // connection error
        if (errno == EAGAIN) {
            return;
        }
        log_error("simple_tcp: recv() fail: %s", strerror(errno));
        simple_tcp_disconnect(tcp);
        if (tcp->listen_fd <= 0) uev_exit(w->ctx);
    } else { // (part of) header read
        buf->pkt->size += (size_t)recv_len;
        if (buf->pkt->size == buf->expecting_length) {
            if (buf->header_parsed) { // body received
                log_debug("simple_tcp: packet body ready");
                pkt_t *pkt = buf->pkt;
                buf->pkt = NULL;
                router_packet_ready(tcp, pkt);
            } else { // header received
                simple_tcp_pkt_header *header = buf->pkt->start;
                buf->pkt->start += buf->pkt->size;
                buf->pkt->size = 0;
                buf->expecting_length = ntohl(header->pkt_size_be);
                buf->header_parsed = true;
                log_debug("simple_tcp: incoming packet of size=%d", buf->expecting_length);
                goto receive_packet;
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
        rem -= (size_t)l;
        pos += (size_t)l;
    }
    return 0;
}

void simple_tcp_fw_pkt(channel_simple_tcp_t *tcp, pkt_t *pkt) {
    if (tcp->channel_fd < 0) {
        log_warn("simple_tcp: tcp channel not ready");
    } else {
        pkt->start -= sizeof(simple_tcp_pkt_header);
        simple_tcp_pkt_header *header = pkt->start;
        header -> pkt_size_be = htonl((uint32_t) pkt->size);
        pkt->size += sizeof(simple_tcp_pkt_header);
        int success = socket_write_sync(tcp->channel_fd, pkt->start, pkt->size);
        if (success < 0) {
            log_fatal("simple_tcp: tcp socket write fail");
            exit(-1);
        }
    }
    router_recycle_pkt(pkt);
}