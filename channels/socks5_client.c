#include "socks5_client.h"
#include "../external/log.h"

#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <fcntl.h>
#include <malloc.h>
#include <errno.h>
#include <string.h>

void socks5_init(socks5_client_t *socks) {
    socks->fd = -1;
}

static int send_exact(int fd, const void *buf, size_t size) {
    size_t rem = size;
    size_t pos = 0;
    while(rem > 0) {
        ssize_t l = send(fd, buf + pos, rem, 0);
        if (l < 0) {
            if (errno == EAGAIN) continue;
            return -1;
        }
        rem -= (size_t)l;
        pos += (size_t)l;
    }
    return 0;
}

static int recv_exact(int fd, void *buf, size_t size) {
    size_t rem = size;
    size_t pos = 0;
    while(rem > 0) {
        ssize_t l = recv(fd, buf + pos, rem, 0);
        if (l < 0) {
            if (errno == EAGAIN) continue;
            return -1;
        }
        rem -= (size_t)l;
        pos += (size_t)l;
    }
    return 0;
}

int socks5_auth(socks5_client_t *socks, const char *socks_host, const char *socks_port) {
    struct addrinfo *peer_addr;
    int fd = -1;

    if (0 != getaddrinfo(socks_host, socks_port, NULL, &peer_addr)) goto err;
    if (0 > (fd = socket(AF_INET, SOCK_STREAM, 0))) goto err;
    if (0 > connect(fd, peer_addr->ai_addr, peer_addr->ai_addrlen)) goto err;
    freeaddrinfo(peer_addr);

    char buf[255];
    char *payload = "\x05\x01\x00"; // hello to server, no auth required
    if (0 > send_exact(fd, payload, 3)) goto err;
    if (0 > recv_exact(fd, buf, 2)) goto err;
    if (buf[0] != '\x05' || buf[1] != '\x00') {
        log_fatal("socks5_client: invalid socks server reply");
        return -1;
    }
    socks->fd = fd;
    return fd;

err:
    if (fd >= 0) close(fd);
    return -1;
}

int socks5_connect(socks5_client_t *socks, const char *remote_host, const char *remote_port) {
    size_t domain_len = strlen(remote_host);
    if (domain_len > 200) {
        log_fatal("socks5_client: peer domain name too long");
        goto err;
    }
    uint16_t port_no = (uint16_t)(atoi(remote_port) & 0xFFFF);
    uint16_t port_no_be = htons(port_no);
    uint8_t payload[256];
    payload[0] = 5; //ver
    payload[1] = 1; //cmd
    payload[2] = 0; //rsv
    payload[3] = 3; //type
    payload[4] = (uint8_t)(domain_len & 0xFFu); // domain len
    memcpy(payload+5, remote_host, domain_len);
    *(uint16_t*)(payload+5+domain_len) = port_no_be;
    size_t payload_length = domain_len + 7;
    if (0 > send_exact(socks->fd, payload, payload_length)) goto err;

    uint8_t recv_buf[512];
    if (0 > recv_exact(socks->fd, recv_buf, 4)) goto err;
    if (recv_buf[0] != 5 || recv_buf[2] != 0) {
        log_fatal("socks5_client: invalid socks5 reply");
        goto err;
    }
    if (recv_buf[1] != 0) {
        log_fatal("socks5_client: CONNECT command fail(%d)", recv_buf[1]);
        goto err;
    }
    if (recv_buf[3] == 1) {
        if (0 > recv_exact(socks->fd, recv_buf+4, 6)) goto err;
    } else if (recv_buf[3] == 4) {
        if (0 > recv_exact(socks->fd, recv_buf+4, 18)) goto err;
    } else if (recv_buf[3] == 3) {
        if (0 > recv_exact(socks->fd, recv_buf+4, 1)) goto err;
        size_t sz = recv_buf[4] + 2u;
        if (0 > recv_exact(socks->fd, recv_buf+5, sz)) goto err;
    } else {
        log_fatal("socks5_client: unknown reply ATYP (blame socks5 server)");
        goto err;
    }
    return 0;

err:
    if (socks->fd > 0) close(socks->fd);
    return -1;
}