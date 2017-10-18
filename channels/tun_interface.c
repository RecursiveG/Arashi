#include "tun_interface.h"

#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>

#include "../external/log.h"

// success return 0
int channel_tun_init(channel_tun_t *tun, const char *device_name, uev_ctx_t *ctx) {
    tun->fd = -1;
    tun->read_watcher = NULL;
    tun->packet_ready = NULL;
    if (strlen(device_name) >= IF_NAMESIZE) {
        log_fatal("device_name too long: %s", device_name);
        exit(-3);
    }

    // open tun device
    int fd = open("/dev/net/tun", O_RDWR, O_NONBLOCK);
    if (fd < 0) return fd;

    // fill in ioctl structure
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN;
    strncpy(ifr.ifr_name, device_name, IF_NAMESIZE);

    // get interface fd
    int err = ioctl(fd, TUNSETIFF, &ifr);
    if (err < 0) {
        close(fd);
        return err;
    } else {
        tun -> fd = fd;
        tun -> read_watcher = malloc(sizeof(uev_t));
        uev_io_init(ctx, tun->read_watcher, channel_tun_incoming_packet_ev, tun, tun->fd, UEV_READ);
        return fd;
    }
}

void channel_tun_close(channel_tun_t *tun) {
    if (tun->read_watcher != NULL) {
        uev_io_stop(tun->read_watcher);
        free(tun->read_watcher);
        tun->read_watcher = NULL;
    }
    if (tun->fd > 0) {
        close(tun->fd);
        tun->fd = -1;
    }
}

void channel_tun_incoming_packet_ev(uev_t *w, void *arg, int events) {
    channel_tun_t *tun = arg;
    size_t buffer_size = sizeof(uint8_t) * (ETH_MAX_MTU + sizeof(struct tun_pi) + 2);
    uint8_t *buffer = malloc(buffer_size);
    ssize_t len = read(tun->fd, buffer, buffer_size);

    if (len < 0) {
        log_fatal("tun read() error: %s", strerror(errno));
        uev_exit(w->ctx);
    } else {
        struct tun_pi *pi = (struct tun_pi *) buffer;
        log_info("Received packet EtherType=0x%04x, size=%ld", ntohs(pi->proto), len - sizeof(*pi));
        if (tun->packet_ready == NULL) {
            log_warn("tun packet_ready is NULL");
        } else {
            tun->packet_ready(tun, buffer, (size_t) len);
        }
    }
    free(buffer);
}

int channel_tun_write(channel_tun_t *tun, const uint8_t buffer[], size_t size) {
    if (tun->fd < 0) {
        log_warn("tun channel not ready");
        return 0;
    }
    ssize_t len = write(tun->fd, buffer, size);
    return len < 0 ? -1 : 0;
}
