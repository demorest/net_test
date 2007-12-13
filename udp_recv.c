#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/times.h>
#include <sys/time.h>
#include <poll.h>
#include <getopt.h>

#include "udp_params.h"

void usage() {
    fprintf(stderr,
            "Usage: udp_recv (options) sender_hostname\n"
            "Options:\n"
            "  -p nn, --port=nn         Port number (%d)\n"
            "  -s nn, --packet-size=nn  Packet size, bytes (%d)\n"
            "  -q, --quiet              More compact output\n"
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
        {"quiet",  0, NULL, 'q'},
        {0,0,0,0}
    };
    int port_num = PORT_NUM;
    int packet_size = PACKET_SIZE;
    int quiet=0;
    int opt, opti;
    while ((opt=getopt_long(argc,argv,"hp:s:q",long_opts,&opti))!=-1) {
        switch (opt) {
            case 'p':
                port_num = atoi(optarg);
                break;
            case 's':
                packet_size = atoi(optarg);
                break;
            case 'q':
                quiet=1;
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

    /* Resolve hostname */
    struct hostent *hh;
    hh = gethostbyname(argv[optind]);
    if (hh==NULL) {
        herror("gethostbyname");
        exit(1);
    }
    //printf("ipaddr=%s\n", inet_ntoa(*(struct in_addr *)hh->h_addr));

    /* Set up address to recieve from */
    struct sockaddr_in ip_addr;
    ip_addr.sin_family = AF_INET;
    ip_addr.sin_port = htons(port_num);
    memcpy(&ip_addr.sin_addr, hh->h_addr, sizeof(struct in_addr));

    /* Bind to local address */
    struct sockaddr_in local_ip;
    local_ip.sin_family = AF_INET;
    local_ip.sin_port = htons(port_num);
    local_ip.sin_addr.s_addr = INADDR_ANY;
    int slen=sizeof(local_ip);
    rv = bind(sock, (struct sockaddr *)&local_ip, slen);
    if (rv==-1) { 
        perror("bind");
        exit(1);
    }

    /* Make recvs non-blocking, set up for polling */
    fcntl(sock, F_SETFL, O_NONBLOCK);
    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = POLLIN;

    /* clock stuff */
    clock_t time0, time1;
    struct tms t0, t1;
    long int tps = sysconf(_SC_CLK_TCK);

    /* Recieve packets */
    double byte_count=0;
    unsigned int packet_count=0;
    unsigned int sent_count=0;
    int drop_count=0;
    unsigned int packet_num=0;
    signal(SIGINT, cc);
    int first=1, timeout=0;
    slen = sizeof(ip_addr);
    while (run) {
        rv = poll(&pfd, 1, 1000);
        if (rv > 0) {
            rv = recvfrom(sock, buf, packet_size, 0,
                    (struct sockaddr *)&ip_addr, &slen);
            if (rv==-1) {
                if (errno!=EAGAIN) { 
                    perror("recvfrom");
                    exit(1);
                }
            } else {
                if (first) { 
                    time0 = times(&t0); 
                    packet_num = *((unsigned int *)buf);
                    sent_count = packet_num;
                    printf("Receiving data.\n");
                    first=0; 
                } else {
                    //drop_count += *((unsigned int *)buf) - (packet_num+1);
                    packet_num = *((unsigned int *)buf);
                    if (packet_num>sent_count) { sent_count=packet_num; }
                }
                packet_count++;
                byte_count += (double)rv;
            }
        } else if (rv==0) {
            if (first==0) { run=0; timeout=1; } /* No data for 1 sec => quit */
        } else {
            perror("poll");
            exit(1);
        }
    }
    time1 = times(&t1);

    double time_sec = (double)(time1-time0)/(double)tps - (double)timeout;
    double load = 
        (double)(t1.tms_utime+t1.tms_stime-t0.tms_utime-t0.tms_stime) /
        (double)(time1-time0);
    double rate = (double)byte_count/time_sec;

    drop_count = sent_count - packet_count;

    if (quiet) {
        printf("%5d %8.1f %8.3f %.3e %5.3f R:%s\n",
                packet_size, byte_count/(1024.0*1024.0), 
                rate/(1024.0*1024.0), (double)drop_count/(double)sent_count,
                load, argv[optind]);
    } else {
        printf("Receiving from %s\n", argv[optind]);
        printf("Packet size %d B\n", packet_size);
        printf("Got %.1f MB\n", byte_count/(1024.0*1024.0)); 
        printf("Rate %.3f MB/s\n", rate/(1024.0*1024.0));
        printf("Dropped %d packets\n", drop_count);
        printf("Drop rate %.3e\n", (double)drop_count/(double)sent_count);
        printf("Avg load %.3f\n", load);
    }

}
