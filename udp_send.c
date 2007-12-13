#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <sys/times.h>
#include <sys/time.h>
#include <unistd.h>
#include <getopt.h>

#include "udp_params.h"

void usage() {
    fprintf(stderr,
            "Usage: udp_send (options) rcvr_hostname\n"
            "Options:\n"
            "  -p nn, --port=nn    Port number (%d)\n"
            "  -s nn, --packet-size=nn  Packet size, bytes (%d)\n"
            , PORT_NUM, PACKET_SIZE);
}

/* Use Ctrl-C for stop */
int run=1;
void cc(int sig) { run=0; }

int main(int argc, char *argv[]) {

    int i;
    int rv;

    /* Cmd line */
    static struct option long_opts[] = {
        {"help",   0, NULL, 'h'},
        {"port",   1, NULL, 'p'},
        {"packet-size",   1, NULL, 's'},
        {0,0,0,0}
    };
    int port_num = PORT_NUM;
    int packet_size = PACKET_SIZE;
    int opt, opti;
    while ((opt=getopt_long(argc,argv,"hp:s:",long_opts,&opti))!=-1) {
        switch (opt) {
            case 'p':
                port_num = atoi(optarg);
                break;
            case 's':
                packet_size = atoi(optarg);
                break;
            case 'h':
            default:
                usage();
                exit(0);
                break;
        }
    }

    /* check args */
    if (optind==argc) {
        usage();
        exit(1);
    }

    /* Create socket */
    int sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock==-1) {
        perror("socket");
        exit(1);
    }

    /* Init buffer, use first 4 bytes as packet count */
    char *buf = (char *)malloc(sizeof(char)*packet_size);
    *((unsigned int *)buf) = 0;

    /* Resolve hostname */
    struct hostent *hh;
    hh = gethostbyname(argv[optind]);
    if (hh==NULL) {
        herror("gethostbyname");
        exit(1);
    }
    //printf("ipaddr=%s\n", inet_ntoa(*(struct in_addr *)hh->h_addr));

    /* Set up recvr address */
    struct sockaddr_in ip_addr;
    ip_addr.sin_family = AF_INET;
    ip_addr.sin_port = htons(port_num);
    memcpy(&ip_addr.sin_addr, hh->h_addr, sizeof(struct in_addr));
    //rv = inet_aton(argv[optind], &ip_addr.sin_addr);
    //if (rv==0) { fprintf(stderr, "Bad IP address.\n"); exit(1); }
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
