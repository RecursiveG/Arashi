#ifndef ARASHI_TUN_INTERFACE_H
#define ARASHI_TUN_INTERFACE_H

#include <sys/types.h>
#include <stdint.h>
#include <uev/uev.h>

struct _channel_tun_t;
typedef void tun_packet_cb(struct _channel_tun_t *tun, uint8_t buffer[], size_t size);

typedef struct _channel_tun_t {
    int fd;
    tun_packet_cb *packet_ready;
} channel_tun_t;

int channel_tun_init(channel_tun_t *tun, const char *device_name);
ssize_t channel_tun_read(const channel_tun_t *tun, uint8_t buffer[], size_t size);
ssize_t channel_tun_write(const channel_tun_t *tun, uint8_t buffer[], size_t size);
void tun_incoming_packet(uev_t *w, void *arg, int events);

#endif //ARASHI_TUN_INTERFACE_H
