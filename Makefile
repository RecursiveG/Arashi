SRC=main.c router.c hexdump.c external/log.c channels/simple_tcp.c channels/tun_interface.c
HEADERS=router.h hexdump.h external/log.h channels/simple_tcp.h channels/tun_interface.h

main:
	gcc -Wall -Wextra -Wno-unused-parameter -DLOG_USE_COLOR -O0 -ggdb $(SRC) -luev -o arashi
