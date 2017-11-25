#include <stdio.h>
#include <uev/uev.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include "channels/tun_interface.h"
#include "channels/simple_tcp.h"
#include "channels/socks5_client.h"
#include "external/log.h"
#include "signal.h"
#include "router.h"

static uev_ctx_t ctx;
static channel_tun_t tun;
static channel_simple_tcp_t tcp;
static socks5_client_t socks5_client;

void cleanup_exit(uev_t *w, void *arg, int events)
{
    if (w != NULL) uev_exit(w->ctx);
    simple_tcp_clean(&tcp);
    channel_tun_close(&tun);
    log_info("Server closed");
}

typedef struct {
    char *tun_dev_name;

    bool tcp_listen; // true=listen, false=connect
    char *tcp_host;
    char *tcp_port;

    bool use_socks;
    char *socks_host;
    char *socks_port;

    bool verbose;
} arg_t;

void print_help(void);
#define CHECK_NEXT_PARAMETER(x) if (++i >= argc) {log_fatal("Missing parameter for \"%s\"", x);return false;}
// return true if parsing success
bool parse_arg(int argc, char *argv[], arg_t *arg) {
    if (argc <= 1) {
        print_help();
        return false;
    }

    arg->tun_dev_name = NULL;
    arg->verbose = false;

    arg->tcp_listen = true;
    arg->tcp_host = NULL;
    arg->tcp_port = NULL;

    arg->use_socks = false;
    arg->socks_host = NULL;
    arg->socks_port = NULL;

    for (ssize_t i = 1; i < argc; i++) {
        if (0 == strncmp("--tun", argv[i], 6)) {
            CHECK_NEXT_PARAMETER("--tun");
            arg->tun_dev_name = argv[i];
        } else if (0 == strncmp("--tcp-connect", argv[i], 14)) {
            CHECK_NEXT_PARAMETER("--tcp-connect")
            CHECK_NEXT_PARAMETER("--tcp-connect")
            arg->tcp_listen = false;
            arg->tcp_host = argv[i-1];
            arg->tcp_port = argv[i];
        } else if (0 == strncmp("--tcp-listen", argv[i], 13)) {
            CHECK_NEXT_PARAMETER("--tcp-listen")
            arg->tcp_listen = true;
            arg->tcp_port = argv[i];
        } else if (0 == strncmp("--via-socks5", argv[i], 13)) {
            CHECK_NEXT_PARAMETER("--via-socks5")
            CHECK_NEXT_PARAMETER("--tcp-connect")
            arg->use_socks = true;
            arg->socks_host = argv[i-1];
            arg->socks_port = argv[i];
        } else if (0 == strncmp("--verbose", argv[i], 10)) {
            arg->verbose = true;
        } else if (0 == strncmp("--help", argv[i], 7)) {
            print_help();
            return false;
        } else {
            log_fatal("Unknown argument \"%s\"", argv[i]);
            return false;
        }
    }

    if (arg->tun_dev_name == NULL) {
        log_fatal("No TUN device specified");
        return false;
    } else if (arg->tcp_port == NULL) {
        log_fatal("No TCP connection specified");
        return false;
    } else if (arg->use_socks && arg->tcp_listen) {
        log_fatal("Socks5 client bind not implemented");
    }

    return true;
}


/* ./arashi
 *     --tun [TUN device name]
 *     --tcp-connect [peer_ip] [peer_port]
 *     --tcp-listen [listen_port]
 *     --via-socks5 [socks_ip] [socks_ip]
 *     --verbose
 */
void print_help(void) {
    printf(        "./arashi\n"
                   "    --tun [TUN device name]\n"
                   "    --tcp-connect [peer_ip] [peer_port]\n"
                   "    --tcp-listen [listen_port]\n"
                   "    --via-socks5 [socks_ip] [socks_ip]\n"
                   "    --verbose\n");
}

int main(int argc, char *argv[]) {
    arg_t arg;
    if (!parse_arg(argc, argv, &arg)) {
        exit(-1);
    }
    if (arg.verbose) {
        log_set_level(LOG_DEBUG);
    } else {
        log_set_level(LOG_INFO);
    }

    uev_init(&ctx);
    uev_t sigterm_watcher, sigint_watcher;
    uev_signal_init(&ctx, &sigterm_watcher, cleanup_exit, NULL, SIGTERM);
    uev_signal_init(&ctx, &sigint_watcher, cleanup_exit, NULL, SIGINT);
    log_info("Event context created");

    const char* tun_name = arg.tun_dev_name;
    int tun_fd = channel_tun_init(&tun, tun_name, &ctx);
    if (tun_fd < 0) {
        log_fatal("failed to open tun device: %s", strerror(errno));
        exit(-1);
    }
    log_info("TUN device allocated: fd = %d", tun_fd);

    simple_tcp_init(&tcp);
    if (arg.tcp_listen) {
        int err = simple_tcp_listen(&tcp, "0.0.0.0", arg.tcp_port, &ctx);
        if (err < 0) {
            log_fatal("failed to open listen socket: %s", strerror(errno));
            exit(-1);
        }
        log_info("Listening socket created");
    } else {
        if (arg.use_socks) {
            socks5_init(&socks5_client);
            log_info("connecting to socks5 server");
            if (0 > socks5_auth(&socks5_client, arg.socks_host, arg.socks_port)) {
                log_fatal("cannot authenticate with socks5 server", strerror(errno));
                exit(-1);
            }
            log_info("connecting to tun server");
            if (0 > socks5_connect(&socks5_client, arg.tcp_host, arg.tcp_port)) {
                log_fatal("cannot connect to remote TUN server", strerror(errno));
                exit(-1);
            }
            if (0 > simple_tcp_via_socks5(&tcp, &socks5_client, &ctx)) {
                log_fatal("cannot connect to remote TUN server", strerror(errno));
                exit(-1);
            }
        } else { // do not use socks5
            if (0 > simple_tcp_connect(&tcp, arg.tcp_host, arg.tcp_port, &ctx)) {
                log_fatal("failed to connect: %s", strerror(errno));
                exit(-1);
            }
        }
        log_info("Connected to peer");
    }

    router_init();
    default_router.ctx = &ctx;
    router_add_forward_channel(&tun);
    router_add_forward_channel(&tcp);
    router_add_backward_channel(&tcp);
    router_add_backward_channel(&tun);

    log_info("Server started");
    uev_run(&ctx, 0);
    cleanup_exit(NULL, NULL, 0);
}
