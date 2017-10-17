SRC=main.c router.c hexdump.c external/log.c channels/simple_tcp.c channels/tun_interface.c
HEADERS=router.h hexdump.h external/log.h channels/simple_tcp.h channels/tun_interface.h

arashi: $(SRC) $(HEADERS)
	gcc -Wall -Wextra -O0 -ggdb $(SRC) -luev -o arashi
