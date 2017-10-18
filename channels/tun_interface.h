#ifndef ARASHI_TUN_INTERFACE_H
#define ARASHI_TUN_INTERFACE_H

#include <sys/types.h>
#include <stdint.h>
#include <uev/uev.h>

typedef struct _channel_tun_t{
    int fd;
    uev_t *read_watcher;
    void (*packet_ready)(struct _channel_tun_t *tcp, const uint8_t buffer[], size_t size);
} channel_tun_t;

int channel_tun_init(channel_tun_t *tun, const char *device_name, uev_ctx_t *ctx);
void channel_tun_close(channel_tun_t *tun);

void channel_tun_incoming_packet_ev(uev_t *w, void *arg, int events);

int channel_tun_write(channel_tun_t *tun, const uint8_t buffer[], size_t size);

#endif //ARASHI_TUN_INTERFACE_H
