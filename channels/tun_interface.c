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
    tun->router_pkt_handler = (accept_pkt_f*)channel_tun_backward_pkt;
    //tun->packet_ready = NULL;
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
        uev_io_init(ctx, tun->read_watcher, channel_tun_forward_event, tun, tun->fd, UEV_READ);
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

void channel_tun_forward_event(uev_t *w, void *arg, int events) {
    channel_tun_t *tun = arg;
    pkt_t *pkt = router_request_pkt();

    uint8_t *buffer = pkt->start;
    ssize_t signed_buffer_size = &(pkt->buf[PKT_BUFFER_SIZE]) - buffer ;
    if (signed_buffer_size <= 0) {
        log_fatal("tun_interface: insufficient buffer size");
        exit(-1);
    }
    size_t buffer_size = (size_t) signed_buffer_size;
    ssize_t len = read(tun->fd, pkt->start, buffer_size);

    if (len < 0) {
        log_fatal("tun read() error: %s", strerror(errno));
        uev_exit(w->ctx);
    } else {
        pkt->size = (size_t)len;
        struct tun_pi *pi = (struct tun_pi *) buffer;
        log_trace("Received packet EtherType=0x%04x, size=%ld", ntohs(pi->proto), (size_t)len - sizeof(*pi));
        router_packet_ready(tun, pkt);
    }
}

void channel_tun_backward_pkt(channel_tun_t *tun, pkt_t *pkt) {
    if (tun->fd < 0) {
        log_warn("tun channel not ready");
    } else {
        ssize_t len = write(tun->fd, pkt->start, pkt->size);
        if (len < 0) {
            log_warn("failed to write tun device");
        }
    }
    router_recycle_pkt(pkt);
}