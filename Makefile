CC = gcc
CFLAGS = -g -O2 -Wall

PROGS = udp_send udp_recv

all: $(PROGS)

install: $(PROGS)
	cp $(PROGS) $(LOCAL)/bin/

clean:
	rm -f $(PROGS) *~ *.o

udp_send:  udp_send.c udp_params.h Makefile
	$(CC) $(CFLAGS) udp_send.c -o udp_send
udp_recv:  udp_recv.c udp_params.h Makefile
	$(CC) $(CFLAGS) udp_recv.c -o udp_recv -lpthread

