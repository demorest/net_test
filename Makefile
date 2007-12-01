CC = gcc

PROGS = net_send net_recv udp_send udp_recv
	
all: $(PROGS)

clean:
	rm -f $(PROGS)

net_send:  net_send.c Makefile
	$(CC) net_send.c -o net_send
net_recv:  net_recv.c Makefile
	$(CC) net_recv.c -o net_recv
udp_send:  udp_send.c udp_params.h Makefile
	$(CC) udp_send.c -o udp_send
udp_recv:  udp_recv.c udp_params.h Makefile
	$(CC) udp_recv.c -o udp_recv

