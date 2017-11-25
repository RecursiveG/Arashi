SRC=main.c router.c hexdump.c external/log.c channels/simple_tcp.c channels/tun_interface.c channels/socks5_client.c
HEADERS=router.h hexdump.h external/log.h channels/simple_tcp.h channels/tun_interface.h channels/socks5_client.h

FLAGS_RELEASE=-Wall -Wextra -Wconversion -Wsign-conversion -Wformat -Wformat-security -Wno-unused-parameter -DLOG_USE_COLOR -O2 -Werror -march=native -fstack-protector-all --param ssp-buffer-size=4 -pie -fPIE -D_FORTIFY_SOURCE=2 -Wl,-z,relro,-z,now
FLAGS_DEBUG=-Wall -Wextra -Wconversion -Wsign-conversion -Wformat -Wformat-security -Wno-unused-parameter -DLOG_USE_COLOR -Og -ggdb

debug:
	gcc $(FLAGS_DEBUG) $(SRC) -luev -o arashi

release:
	gcc $(FLAGS_RELEASE) $(SRC) -luev -o arashi
