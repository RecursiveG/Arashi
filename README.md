# Project Arashi

You need to setup the TUN device manually.

    sudo ip tuntap add mode tun user <login> name <tun_name>
    sudo ip addr add <ip_addr>/<prefix> dev <tun_name>
    sudo ip link set <tun_name> up

    make
    ./arashi --tun <tun_name> --tcp-listen <listen_port>                # server side
    ./arashi --tun <tun_name> --tcp-connect <server_addr> <listen_port> # client side
