#include <stdio.h>
#include <uev/uev.h>
#include <linux/ip.h>
#include <linux/if_tun.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "channels/tun_interface.h"
#include "channels/simple_tcp.h"
#include "hexdump.h"
#include "external/log.h"
#include "signal.h"
#include "router.h"

void cleanup_exit(uev_t *w, void *arg, int events)
{
    /* Graceful exit, with optional cleanup ... */
    uev_exit(w->ctx);
    log_info("Server closed");
}

int main(int argc, char *argv[]) {
    if (argc != 3 && argc != 4) {
        printf("Usage: %s <TUN_device_name> <listen_port>\n", argv[0]);
        printf("Usage: %s <TUN_device_name> <peer> <peer_port>\n", argv[0]);
        return -1;
    }

    uev_ctx_t ctx;
    uev_init(&ctx);
    uev_t sigterm_watcher, sigint_watcher;
    uev_signal_init(&ctx, &sigterm_watcher, cleanup_exit, NULL, SIGTERM);
    uev_signal_init(&ctx, &sigint_watcher, cleanup_exit, NULL, SIGINT);
    log_info("Event context created");

    const char* tun_name = argv[1];
    channel_tun_t tun;
    int tun_fd = channel_tun_init(&tun, tun_name);
    if (tun_fd < 0) {
        log_fatal("failed to open tun device: %s", strerror(errno));
        exit(-1);
    }
    uev_t tun_watcher;
    uev_io_init(&ctx, &tun_watcher, tun_incoming_packet, &tun, tun.fd, UEV_READ);
    log_info("TUN device allocated: fd = %d", tun_fd);

    channel_simple_tcp_t tcp;
    simple_tcp_init(&tcp);
    tcp.default_packet_ready = on_packet_from_simple_tcp;
    if (argc == 3) {
        int err = simple_tcp_listen(&tcp, "0.0.0.0", argv[2]);
        if (err < 0) {
            log_fatal("failed to open listen socket: %s", strerror(errno));
            if (tcp.listen_fd >= 0) close(tcp.listen_fd);
            exit(-1);
        }
        uev_t *w = malloc(sizeof(*w));
        uev_io_init(&ctx, w, simple_tcp_incoming_conn, &tcp, tcp.listen_fd, UEV_READ);
        log_info("Listening socket created");
    } else {
        int channel_id = simple_tcp_connect(&tcp, argv[2], argv[3]);
        if (channel_id < 0) {
            log_fatal("failed to connect: %s", strerror(errno));
            exit(-1);
        }
        uev_t *w = malloc(sizeof(*w));
        uev_io_init(&ctx, w, simple_tcp_incoming_data, tcp.channels[channel_id], tcp.channels[channel_id]->fd, UEV_READ);
        log_info("Connected to peer");
    }

    default_router.tcp = &tcp;
    default_router.tun = &tun;
    tun.packet_ready = on_packet_from_tun;

    log_info("Server started");
    uev_run(&ctx, 0);
}
