#include "router.h"

#include <stdlib.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <memory.h>
#include "external/log.h"

router_t default_router;

static ssize_t write_exact_socket(int socket, uint8_t buf[], size_t size) {
    size_t rem = size;
    size_t pos = 0;
    while(rem > 0) {
        ssize_t l = send(socket, buf + pos, rem, 0);
        if (l < 0 && errno != EAGAIN) {
            return -1;
        } else if (l >= 0) {
            rem -= l;
            pos += l;
        }
    }
    return size;
}

void router_init() {
    default_router.tun = NULL;
    default_router.tcp = NULL;
}

void on_packet_from_tun(channel_tun_t *tun, uint8_t buffer[], size_t size) {
    if (tun != default_router.tun) {
        log_error("router received packet from unregistered tun device");
        return;
    }
    if (default_router.tcp == NULL || default_router.tcp->opened_channels <= 0) {
        log_error("no tcp channel available");
        return;
    }
    channel_simple_tcp_conn_t *conn = default_router.tcp->channels[0];
    *(uint16_t*)buffer = htons((uint16_t)size); // magic tun_incoming_packet
    if (write_exact_socket(conn->fd, buffer, size+2)<0) {
        log_fatal("tcp write fail: %s", strerror(errno));
        exit(-2);
    }
}

void on_packet_from_simple_tcp(channel_simple_tcp_conn_t *conn, void *buf, size_t size) {
    if (default_router.tcp == NULL || default_router.tcp->channels[0] != conn) {
        log_error("router received packet from unregistered tcp channel");
        return;
    }

    if (default_router.tun == NULL) {
        log_error("no tun devices available");
        return;
    }

    if (write(default_router.tun->fd, buf, size)<0) {
        log_fatal("tun write fail: %s", strerror(errno));
        exit(-2);
    }
}
