#ifndef ARASHI_SOCKS5_CLIENT_H
#define ARASHI_SOCKS5_CLIENT_H

typedef struct {
    int fd;
} socks5_client_t;

void socks5_init(socks5_client_t *socks);
int socks5_auth(socks5_client_t *socks, const char *socks_host, const char *socks_port);
int socks5_connect(socks5_client_t *socks, const char *remote_host, const char *remote_port);

#endif //ARASHI_SOCKS5_CLIENT_H
