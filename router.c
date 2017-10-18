#include "router.h"

#include <stdlib.h>
#include <errno.h>
#include <memory.h>
#include "external/log.h"

router_t default_router;

void router_init() {
    default_router.tun = NULL;
    default_router.tcp = NULL;
    default_router.ctx = NULL;
}

static const uint8_t XOR_MAGIC=0x5A;
void on_packet_from_tun(channel_tun_t *tun, const uint8_t *buf, size_t size) {
    if (default_router.tcp == NULL || default_router.tcp->channel_fd < 0) {
        log_error("no tcp channel available");
        return;
    }
    uint8_t *b = malloc(size);
    for (size_t i=0;i<size;i++) b[i] = buf[i] ^ XOR_MAGIC;
    if (simple_tcp_write(default_router.tcp, b, size)<0) {
        log_error("tcp device write fail: %s", strerror(errno));
        simple_tcp_disconnect(default_router.tcp);
        if (default_router.tcp->channel_fd < 0) {
            uev_exit(default_router.ctx);
        }
    }
}

void on_packet_from_simple_tcp(channel_simple_tcp_t *conn, const uint8_t *buf, size_t size) {
    if (default_router.tun == NULL) {
        log_error("no tun devices available");
        return;
    }
    uint8_t *b = malloc(size);
    for (size_t i=0;i<size;i++) b[i] = buf[i] ^ XOR_MAGIC;
    if (channel_tun_write(default_router.tun, b, size)<0) {
        log_fatal("tun device write fail: %s", strerror(errno));
        uev_exit(default_router.ctx);
    }
}
