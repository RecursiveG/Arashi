#ifndef ARASHI_SIMPLE_TCP_H
#define ARASHI_SIMPLE_TCP_H

#include <stdint.h>
#include <uev/uev.h>
#include <linux/if_ether.h>

#define SIMPLE_TCP_MAX_PACKET ETH_MAX_MTU
#define SIMPLE_TCP_HEADER_LEN sizeof(simple_tcp_pkt_header)

typedef struct {
    uint32_t pkt_size_be;
} simple_tcp_pkt_header;

typedef struct {
    uint32_t pkt_size; // header size not included if recv_buf but included if send_buf
    uint32_t processed_size;
    struct {
        simple_tcp_pkt_header header;
        uint8_t body[SIMPLE_TCP_MAX_PACKET];
    } pkt;
} simple_tcp_buf;

typedef struct _channel_simple_tcp_t{
    int listen_fd;
    int channel_fd;

    uev_t *listen_watcher;
    uev_t *recv_watcher;

    simple_tcp_buf recv_buf;

    void (*packet_ready)(struct _channel_simple_tcp_t *tcp, const uint8_t buffer[], size_t size);
} channel_simple_tcp_t;

void simple_tcp_init(channel_simple_tcp_t *tcp);
void simple_tcp_clean(channel_simple_tcp_t *tcp);

int simple_tcp_listen(channel_simple_tcp_t *tcp, const char *addr, const char *service, uev_ctx_t *ctx);
void simple_tcp_deafen(channel_simple_tcp_t *tcp);
int simple_tcp_connect(channel_simple_tcp_t *tcp, const char *addr, const char *service, uev_ctx_t *ctx);
void simple_tcp_disconnect(channel_simple_tcp_t *tcp);

// libuev callbacks
void simple_tcp_incoming_conn_ev(uev_t *w, void *arg, int events);
void simple_tcp_incoming_data_ev(uev_t *w, void *arg, int events);

int simple_tcp_write(channel_simple_tcp_t *tcp, const uint8_t buffer[], size_t size);

#endif //ARASHI_SIMPLE_TCP_H
