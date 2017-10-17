#ifndef ARASHI_SIMPLE_TCP_H
#define ARASHI_SIMPLE_TCP_H

#include <stdint.h>
#include <sys/types.h>
#include <uev/uev.h>

#define SIMPLE_TCP_MAX_CONNCURRENT 16
#define SIMPLE_TCP_MAX_PACKET 0xFFFFU
#define SIMPLE_TCP_HEADER_LEN sizeof(uint16_t)

struct _channel_simple_tcp_conn_t;
// the size here does not contain SIMPLE_TCP_HEADER
typedef void (simple_tcp_pkt_ready_cb)(struct _channel_simple_tcp_conn_t *conn, void *buf, size_t size);

typedef struct {
    uint16_t pkt_size_be; // in network endian
    uint8_t pkt[SIMPLE_TCP_MAX_PACKET];
} simple_tcp_pkt;

typedef struct _channel_simple_tcp_conn_t {
    int fd;
    //simple_tcp_pkt send_buf;
    simple_tcp_pkt recv_buf;

    // all sizes does not contain SIMPLE_TCP_HEADER_LEN;
    //size_t send_size; // if not 0, an packet is incoming
    //size_t sent_size; // how much has received so far
    size_t recv_size;
    size_t recd_size;

    simple_tcp_pkt_ready_cb *packet_ready;
} channel_simple_tcp_conn_t;

typedef struct {
    int listen_fd;
    int opened_channels;
    simple_tcp_pkt_ready_cb *default_packet_ready;
    channel_simple_tcp_conn_t* channels[SIMPLE_TCP_MAX_CONNCURRENT];
} channel_simple_tcp_t;

void simple_tcp_init(channel_simple_tcp_t *tcp);
int simple_tcp_listen(channel_simple_tcp_t *tcp, const char *addr, const char *service);
int simple_tcp_connect(channel_simple_tcp_t *tcp, const char *addr, const char *service);

void simple_tcp_incoming_conn(uev_t *w, void *arg, int events);
void simple_tcp_incoming_data(uev_t *w, void *arg, int events);

#endif //ARASHI_SIMPLE_TCP_H
