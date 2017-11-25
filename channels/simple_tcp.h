#ifndef ARASHI_SIMPLE_TCP_H
#define ARASHI_SIMPLE_TCP_H

#include <stdint.h>
#include <stdbool.h>
#include <uev/uev.h>
#include <linux/if_ether.h>
#include "../router.h"
#include "socks5_client.h"

typedef struct {
    uint32_t pkt_size_be;
} simple_tcp_pkt_header;

typedef struct {
    pkt_t *pkt;
    uint32_t expecting_length;
    bool header_parsed;
} simple_tcp_buf;

typedef struct _channel_simple_tcp_t{
    accept_pkt_f *router_pkt_handler;

    int listen_fd;
    int channel_fd;

    uev_t *listen_watcher;
    uev_t *recv_watcher;

    simple_tcp_buf recv_buf;
} channel_simple_tcp_t;

void simple_tcp_init(channel_simple_tcp_t *tcp);
void simple_tcp_clean(channel_simple_tcp_t *tcp);

int simple_tcp_listen(channel_simple_tcp_t *tcp, const char *addr, const char *service, uev_ctx_t *ctx);
void simple_tcp_deafen(channel_simple_tcp_t *tcp);
int simple_tcp_connect(channel_simple_tcp_t *tcp, const char *addr, const char *service, uev_ctx_t *ctx);
void simple_tcp_disconnect(channel_simple_tcp_t *tcp);

int simple_tcp_via_socks5(channel_simple_tcp_t *tcp, socks5_client_t *socks5_client, uev_ctx_t *ctx);

// libuev callbacks
void simple_tcp_bw_conn_ev(uev_t *w, void *arg, int events);
void simple_tcp_bw_data_ev(uev_t *w, void *arg, int events);

void simple_tcp_fw_pkt(channel_simple_tcp_t *tcp, pkt_t *pkt);

#endif //ARASHI_SIMPLE_TCP_H
