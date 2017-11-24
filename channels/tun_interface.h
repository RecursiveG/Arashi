#ifndef ARASHI_TUN_INTERFACE_H
#define ARASHI_TUN_INTERFACE_H

#include <sys/types.h>
#include <stdint.h>
#include <uev/uev.h>
#include "../router.h"

typedef struct _channel_tun_t{
    accept_pkt_f *router_pkt_handler;
    int fd;
    uev_t *read_watcher;
} channel_tun_t;

int channel_tun_init(channel_tun_t *tun, const char *device_name, uev_ctx_t *ctx);
void channel_tun_close(channel_tun_t *tun);

void channel_tun_forward_event(uev_t *w, void *arg, int events);
void channel_tun_backward_pkt(channel_tun_t *tun, pkt_t *pkt);

#endif //ARASHI_TUN_INTERFACE_H
