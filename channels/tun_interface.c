#include "tun_interface.h"

#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/if_ether.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>

#include "../external/log.h"

// success return 0
int channel_tun_init(channel_tun_t *tun, const char *device_name) {
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
        return fd;
    }
}

// return actual size read.
ssize_t channel_tun_read(const channel_tun_t *tun, uint8_t buffer[], size_t size) {
    if (size < 0) {
        log_error("invalid write size: %ld", size);
        return -2;
    }

    ssize_t len = read(tun->fd, buffer, size);
    if (len < 0) return -1;
    if (len >= size) {
        log_warn("buffer full, read maybe incomplete %ld >= %ld", len, size);
    }
    return len;
}

// success return size, syscall failure -1, other failure -2
// NOTE: the buffer should contain the tun_pi header
ssize_t channel_tun_write(const channel_tun_t *tun, uint8_t buffer[], size_t size) {
    if (size < 0 || size > ETH_MAX_MTU) {
        log_error("invalid write size: %ld", size);
        return -2;
    }

    ssize_t err = write(tun->fd, buffer, size);
    if (err < 0) {
        return -1;
    } else {
        return 0;
    }
}

void tun_incoming_packet(uev_t *w, void *arg, int events) {
    channel_tun_t *tun = arg;

    uint8_t packet[2 + ETH_MAX_MTU]; // magic
    ssize_t pkt_size = channel_tun_read(tun, packet + 2, ETH_MAX_MTU);
    if (pkt_size < 0) {
        log_error("tun read error: %s", strerror(errno));
        uev_io_stop(w);
        return;
    }

    struct tun_pi *pi = (struct tun_pi*) packet + 2;
    log_info("Received packet proto=%d, size=%ld", pi->proto, pkt_size - sizeof(*pi));

    if (tun->packet_ready == NULL)
        log_warn("tun packet_ready is NULL");
    else
        tun->packet_ready(tun, packet, (size_t)pkt_size);
}
