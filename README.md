# Arashi

You need to setup the TUN device manually.

    sudo ip tuntap add mode tun user <login> name <tun_name>
    sudo ip addr add <ip_addr>/<prefix> dev <tun_name>
    sudo ip link set <tun_name> up

Options:

    ./arashi
        --tun [TUN device name]
        --tcp-connect [peer_ip] [peer_port]
        --tcp-listen [listen_port]
        --via-socks5 [socks_ip] [socks_ip]
        --verbose

Examples:

    ./arashi --tun <tun_name> --tcp-listen <listen_port>                # server side
    ./arashi --tun <tun_name> --tcp-connect <server_addr> <listen_port> # client side
    ./arashi --tun <tun_name> --tcp-connect <server_addr> <listen_port> \
                             --via-socks5 <socks5_server> <socks5_port> # client connect via socks5 proxy
                             
Note:

- No encryption (yet).
- No auto reconnect (yet).
- Do not use on socks5 servers you don't trust.
- Use at your risk.
    
