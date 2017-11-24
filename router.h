#ifndef ARASHI_ROUTER_C_H
#define ARASHI_ROUTER_C_H

#include <sys/types.h>
#include <uev/uev.h>

#define PKT_BUFFER_SIZE 0x20000
typedef struct {
    size_t nbr;
    void *start;
    size_t size;
    uint8_t buf[PKT_BUFFER_SIZE];
} pkt_t;

#define ROUTER_CHAIN_LEN 16
typedef struct {
    uev_ctx_t *ctx;

    size_t chain_forward_len;
    size_t chain_backward_len;
    void *chain_forward[ROUTER_CHAIN_LEN];
    void *chain_backward[ROUTER_CHAIN_LEN];

    size_t pkt_pool_size;
    size_t pkt_queue_head;
    size_t pkt_queue_tail;
    pkt_t **pkt_allocated;
    pkt_t **pkt_queue;
} router_t;

// To be implemented by channels, must be the first member of channel struct
typedef void(accept_pkt_f)(void *channel, pkt_t *pkt);

extern router_t default_router;

void router_init();
void router_packet_ready(void* src_channel, pkt_t *pkt);
pkt_t* router_request_pkt(void);
void router_recycle_pkt(pkt_t *pkt);

void router_add_forward_channel(void *channel);
void router_add_backward_channel(void *channel);

#endif //ARASHI_ROUTER_C_H
