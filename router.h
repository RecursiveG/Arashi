#ifndef ARASHI_ROUTER_C_H
#define ARASHI_ROUTER_C_H

#include "channels/simple_tcp.h"
#include "channels/tun_interface.h"

typedef struct {
    channel_tun_t *tun;
    channel_simple_tcp_t *tcp;
    uev_ctx_t *ctx;
} router_t;

extern router_t default_router;

void router_init();
void on_packet_from_tun(channel_tun_t *tun, const uint8_t *buf, size_t size);
void on_packet_from_simple_tcp(channel_simple_tcp_t *conn, const uint8_t *buf, size_t size);

#endif //ARASHI_ROUTER_C_H
