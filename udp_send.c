#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/in.h>
#include <signal.h>
#include <sys/times.h>
#include <sys/time.h>
#include <unistd.h>

#include "udp_params.h"

/* Use Ctrl-C for stop */
int run=1;
void cc(int sig) { run=0; }

int main(int argc, char *argv[]) {

    int i;
    int rv;

    /* check args */
    if (argc<2) {
        fprintf(stderr, "Usage: udp_send ip_address\n");
        exit(1);
    }

    /* Create socket */
    int sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock==-1) {
        perror("socket");
        exit(1);
    }

    /* Init buffer, use first 4 bytes as packet count */
    int packet_size = PACKET_SIZE;
    char *buf = (char *)malloc(sizeof(char)*packet_size);
    *((unsigned int *)buf) = 0;

    /* Set up recvr address */
    struct sockaddr_in ip_addr;
    ip_addr.sin_family = AF_INET;
    ip_addr.sin_port = htons(5000);
    rv = inet_aton(argv[1], &ip_addr.sin_addr);
    if (rv==0) { fprintf(stderr, "Bad IP address.\n"); exit(1); }
    int slen = sizeof(ip_addr);

    /* clock stuff */
    clock_t time0, time1;
    struct tms t0, t1;
    long int tps = sysconf(_SC_CLK_TCK);

    /* Send packets */
    int loop_count=10;
    double byte_count=0;
    signal(SIGINT, cc);
    time0 = times(&t0);
    while (run) {
        (*((unsigned int *)buf))++;
        rv = sendto(sock, buf, (size_t)packet_size, 0, 
                (struct sockaddr *)&ip_addr, slen);
        if (rv==-1) {
            perror("sendto");
            exit(1);
        }
        byte_count += (double)packet_size;
    }
    time1 = times(&t1);

    double rate = ((double)byte_count)*((double)tps)/(double)(time1-time0);
    double load = 
        (double)(t1.tms_utime+t1.tms_stime-t0.tms_utime-t0.tms_stime) /
        (double)(time1-time0);


    printf("Sent %.1f MB\n", byte_count/(1024.0*1024.0)); 
    printf("Rate %.3f MB/s\n", rate/(1024.0*1024.0));
    printf("Avg load %.3f\n", load);


}
