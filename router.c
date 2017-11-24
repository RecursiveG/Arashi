#include "router.h"

#include <stdlib.h>
#include "external/log.h"

router_t default_router;

#define DEFAULT_PKT_POOL_SIZE 16
void router_init() {
    router_t* const r = &default_router;
    r->ctx = NULL;

    r->chain_backward_len = 0;
    r->chain_forward_len = 0;
    for (int i=0;i<ROUTER_CHAIN_LEN;i++) {
        r->chain_forward[i] = NULL;
        r->chain_backward[i] = NULL;
    }

    r->pkt_pool_size = DEFAULT_PKT_POOL_SIZE;
    r->pkt_allocated = malloc(sizeof(pkt_t*) * DEFAULT_PKT_POOL_SIZE);
    r->pkt_queue = malloc(sizeof(pkt_t*) * (DEFAULT_PKT_POOL_SIZE + 1));
    for (size_t i=0;i<DEFAULT_PKT_POOL_SIZE;i++) {
        pkt_t *p = malloc(sizeof(pkt_t));
        p->nbr = i;
        p->size = 0;
        p->start = &(p->buf[0x8000]);

        r->pkt_allocated[i] = p;
        r->pkt_queue[i] = p;
    }
    r->pkt_queue_head = 0;
    r->pkt_queue_tail = DEFAULT_PKT_POOL_SIZE;
}

// TODO performance
void router_packet_ready(void* src_channel, pkt_t *pkt) {
    for (size_t i=0;i<default_router.chain_forward_len-1;i++) {
        if (default_router.chain_forward[i] == src_channel) {
            void *channel = default_router.chain_forward[i+1];
            accept_pkt_f *fp = *(accept_pkt_f**)channel;
            fp(channel, pkt);
            return;
        }
    }
    for (size_t i=0;i<default_router.chain_backward_len-1;i++) {
        if (default_router.chain_backward[i] == src_channel) {
            void *channel = default_router.chain_backward[i+1];
            accept_pkt_f *fp = *(accept_pkt_f**)channel;
            fp(channel, pkt);
            return;
        }
    }
}

pkt_t* router_request_pkt(void) {
    if (default_router.pkt_queue_tail == default_router.pkt_queue_head) {
        log_fatal("Not enough packets in queue. Queue expansion not implemented");
        exit(-1);
        // TODO queue expansion
    }
    pkt_t *p = default_router.pkt_queue[default_router.pkt_queue_head];
    p->size = 0;
    p->start = &(p->buf[0x8000]);
    default_router.pkt_queue_head = (default_router.pkt_queue_head + 1) % (default_router.pkt_pool_size + 1);
    return p;
}

void router_recycle_pkt(pkt_t *pkt) {
    if (pkt == NULL) {
        log_fatal("NULL packets cannot be recycled.");
        exit(-1);
    }
    if (default_router.pkt_queue_head == (default_router.pkt_queue_tail + 1) % (default_router.pkt_pool_size + 1)) {
        log_fatal("Queue full. Something went wrong.");
        exit(-1);
    }
    default_router.pkt_queue[default_router.pkt_queue_tail] = pkt;
    default_router.pkt_queue_tail = (default_router.pkt_queue_tail + 1) % (default_router.pkt_pool_size + 1);
}

void router_add_forward_channel(void *channel) {
    if (default_router.chain_forward_len >= ROUTER_CHAIN_LEN) {
        log_fatal("Too many forward channels");
        exit(-1);
    }
    default_router.chain_forward[default_router.chain_forward_len++] = channel;
}

void router_add_backward_channel(void *channel) {
    if (default_router.chain_backward_len >= ROUTER_CHAIN_LEN) {
        log_fatal("Too many backward channels");
        exit(-1);
    }
    default_router.chain_backward[default_router.chain_backward_len++] = channel;
}
