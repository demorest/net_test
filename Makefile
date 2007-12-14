CC = gcc
CFLAGS = -g -O2 -Wall

PROGS = net_send net_recv udp_send udp_recv

all: $(PROGS)

install: $(PROGS)
	cp $(PROGS) $(LOCAL)/bin/

clean:
	rm -f $(PROGS) *~ *.o

net_send:  net_send.c Makefile
	$(CC) $(CFLAGS) net_send.c -o net_send
net_recv:  net_recv.c Makefile
	$(CC) $(CFLAGS) net_recv.c -o net_recv
udp_send:  udp_send.c udp_params.h Makefile
	$(CC) $(CFLAGS) udp_send.c -o udp_send
udp_recv:  udp_recv.c udp_params.h Makefile
	$(CC) $(CFLAGS) udp_recv.c -o udp_recv

