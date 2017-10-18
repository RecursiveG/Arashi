#include <stdio.h>
#include <uev/uev.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "channels/tun_interface.h"
#include "channels/simple_tcp.h"
#include "external/log.h"
#include "signal.h"
#include "router.h"

static uev_ctx_t ctx;
static channel_tun_t tun;
static channel_simple_tcp_t tcp;

void cleanup_exit(uev_t *w, void *arg, int events)
{
    if (w != NULL) uev_exit(w->ctx);
    simple_tcp_clean(&tcp);
    channel_tun_close(&tun);
    log_info("Server closed");
}

int main(int argc, char *argv[]) {
    if (argc != 3 && argc != 4) {
        printf("Usage: %s <TUN_device_name> <listen_port>\n", argv[0]);
        printf("Usage: %s <TUN_device_name> <peer> <peer_port>\n", argv[0]);
        return -1;
    }

    uev_init(&ctx);
    uev_t sigterm_watcher, sigint_watcher;
    uev_signal_init(&ctx, &sigterm_watcher, cleanup_exit, NULL, SIGTERM);
    uev_signal_init(&ctx, &sigint_watcher, cleanup_exit, NULL, SIGINT);
    log_info("Event context created");

    const char* tun_name = argv[1];
    int tun_fd = channel_tun_init(&tun, tun_name, &ctx);
    if (tun_fd < 0) {
        log_fatal("failed to open tun device: %s", strerror(errno));
        exit(-1);
    }
    log_info("TUN device allocated: fd = %d", tun_fd);

    simple_tcp_init(&tcp);
    if (argc == 3) {
        int err = simple_tcp_listen(&tcp, "0.0.0.0", argv[2], &ctx);
        if (err < 0) {
            log_fatal("failed to open listen socket: %s", strerror(errno));
            exit(-1);
        }
        log_info("Listening socket created");
    } else {
        int channel_id = simple_tcp_connect(&tcp, argv[2], argv[3], &ctx);
        if (channel_id < 0) {
            log_fatal("failed to connect: %s", strerror(errno));
            exit(-1);
        }
        log_info("Connected to peer");
    }

    router_init();
    default_router.ctx = &ctx;
    default_router.tcp = &tcp;
    default_router.tun = &tun;
    tcp.packet_ready = on_packet_from_simple_tcp;
    tun.packet_ready = on_packet_from_tun;

    log_info("Server started");
    uev_run(&ctx, 0);
    cleanup_exit(NULL, NULL, 0);
}
