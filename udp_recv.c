/* udp_recv.c
 * Receiver for UDP tests.
 * Paul Demorest 12-15-2007.
 */
#define _GNU_SOURCE 1  /* sched stuff doesn't work w/o this... */
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
#include <pthread.h>
#include <sched.h>

#include "udp_params.h"

void usage() {
    fprintf(stderr,
            "Usage: udp_recv (options) sender_hostname\n"
            "Options:\n"
            "  -p nn, --port=nn            Port number (%d)\n"
            "  -s nn, --packet-size=nn     Packet size, bytes (%d)\n"
            "  -b nn, --buffer-size=nn     Receiver buffer size, packets (2)\n"
            "  -d file, --disk-output=file Output raw data to file\n"
            "  -c n, --cpu=n               Use only CPU #n\n"
            "  -q, --quiet                 More compact output\n"
            "  -e, --endian                Byte-swap seq num\n"
            "  -a, --print                 Print packet seq nums\n"
            "  -t nn, --timeout=nn         Receive timeout, ms (1000)\n"
            "  -n nn, --npacket=nn         Stop after n packets\n"
            , PORT_NUM, PACKET_SIZE);
}

/* Use Ctrl-C for stop */
int run=1;
void cc(int sig) { run=0; }

struct write_info {
    //int fd;
    FILE *fd;
    char *buf;
    size_t nbytes;
};
void *write_to_disk(void *args) {
    struct write_info *inf = (struct write_info *)args;
    size_t rv; 
    size_t remain = inf->nbytes;
    char *wptr = inf->buf;
    while (remain>0) { 
        //rv = write(inf->fd, wptr, remain);
        rv = fwrite(wptr, 1, remain, inf->fd);
        if (rv<0) {
            perror("write");
            exit(1);
        }
        remain -= rv;
        wptr += rv;
    }
    pthread_exit(NULL);
}

void byte_swap(unsigned long long *d) {
    unsigned long long tmp;
    char *ptr1, *ptr2;
    ptr1 = (char *)d;
    ptr2 = (char *)&tmp + 7;
    int i;
    for (i=0; i<8; i++) {
        ptr1 = (char *)d + i;
        ptr2 = (char *)&tmp + 7 - i;
        memcpy(ptr2, ptr1, 1);
    }
    *d = tmp;
}

int main(int argc, char *argv[]) {

    int rv;

    /* Cmd line */
    static struct option long_opts[] = {
        {"help",   0, NULL, 'h'},
        {"port",   1, NULL, 'p'},
        {"packet-size",   1, NULL, 's'},
        {"buffer-size",   1, NULL, 'b'},
        {"disk-output",   1, NULL, 'd'},
        {"cpu",    1, NULL, 'c'},
        {"quiet",  0, NULL, 'q'},
        {"endian", 0, NULL, 'e'},
        {"print",  0, NULL, 'a'},
        {"timeout",  1, NULL, 't'},
        {"npacket",  1, NULL, 'n'},
        {0,0,0,0}
    };
    int port_num = PORT_NUM;
    int packet_size = PACKET_SIZE;
    int buffer_size = 2;
    int disk_out=0; char ofile[1024];
    int cpu_idx=-1;
    int quiet=0, endian=0, print_all=0;
    int poll_timeout=1000, npacket=0;
    int opt, opti;
    while ((opt=getopt_long(argc,argv,"hp:s:qb:d:c:eat:n:",long_opts,&opti))!=-1) {
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
            case 'b':
                buffer_size = atoi(optarg);
                break;
            case 'd':
                disk_out=1;
                strncpy(ofile,optarg,1024);
                break;
            case 'c':
                cpu_idx = atoi(optarg);
                break;
            case 'e':
                endian=1;
                break;
            case 'a':
                print_all=1;
                break;
            case 't':
                poll_timeout = atoi(optarg);
                break;
            case 'n':
                npacket = atoi(optarg);
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

    /* Set CPU affinity */
    cpu_set_t cpuset, cpuset_orig;
    if (cpu_idx>=0) {
        /* get current list */
        sched_getaffinity(0, sizeof(cpu_set_t), &cpuset_orig);
        /* blank out new list */
        CPU_ZERO(&cpuset);
        /* Add requested CPU */
        CPU_SET(cpu_idx, &cpuset);
        /* use new settings */
        rv = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
        if (rv<0) { 
            perror("sched_setaffinity");
            exit(1);
        }
    }

    /* Create socket */
    int sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock==-1) {
        perror("socket");
        exit(1);
    }

    /* Test that buffer_size is reasonable */
    long long buffer_size_bytes = (long long)packet_size * 
        (long long)buffer_size;
    if (buffer_size<0) { 
        fprintf(stderr, "Buffer size is negative!\n");
        exit(1);
    }
    if (buffer_size_bytes>1024*1024*1024) { /* 1 GB max */
        fprintf(stderr, "Max buffer size is 1 GB\n");
        exit(1);
    }

    /* Init buffer, use first 4 bytes as packet count */
    char *buf = (char *)malloc(sizeof(char)*packet_size*buffer_size);

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
    unsigned slen=sizeof(local_ip);
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

    /* Open output file if needed */
    int first_write=1;
    pthread_t write_tid;
    struct write_info out;
    if (disk_out) {
        //out.fd = open(ofile, O_RDWR | O_CREAT | O_LARGEFILE);
        //out.fd = open(ofile, O_RDWR | O_CREAT, 0666);
        out.fd = fopen(ofile, "wb");
        if (out.fd==NULL) { fprintf(stderr, "you lose.\n"); exit(1); }
        out.nbytes = buffer_size_bytes/2;
    }

    /* clock stuff */
    clock_t time0, time1;
    struct tms t0, t1;
    long int tps = sysconf(_SC_CLK_TCK);

    /* Recieve packets */
    double byte_count=0;
    unsigned long long packet_count=0;
    unsigned long long sent_count=0;
    int drop_count=0;
    unsigned long long packet_0=0, packet_num=0, last_packet_num=0;
    signal(SIGINT, cc);
    int first=1, timeout=0;
    slen = sizeof(ip_addr);
    int bufctr=0;
    char *bufptr=buf;
    while (run) {
        rv = poll(&pfd, 1, poll_timeout);
        if (rv > 0) {
            rv = recvfrom(sock, bufptr, packet_size, 0,
                    (struct sockaddr *)&ip_addr, &slen);
            if (rv==-1) {
                if (errno!=EAGAIN) { 
                    perror("recvfrom");
                    exit(1);
                }
            } else {
                if (first) { 
                    time0 = times(&t0); 
                    packet_num = *((unsigned long long *)bufptr);
                    if (endian) byte_swap(&packet_num);
                    sent_count = packet_num;
                    packet_0 = packet_num;
                    fprintf(stderr, "Receiving data (size=%d).\n", rv);
                    first=0;
                } else {
                    //drop_count += *((unsigned int *)buf) - (packet_num+1);
                    packet_num = *((unsigned long long *)bufptr);
                    if (endian) byte_swap(&packet_num);
                    if (packet_num>sent_count) { sent_count=packet_num; }
                }

                /* Test, print packet_num */
                if (print_all)  {
                    int i;
                    for (i=0; i<8; i++) {
                        printf("%2.2X ", *(unsigned char *)&bufptr[i]);
                    }
                    printf("%20lld (diff=%d)\n", packet_num,
                            packet_num-last_packet_num);
                }

                /* Update counters, pointers */
                packet_count++;
                byte_count += (double)rv;
                bufctr++;
                if (bufctr >= buffer_size) {
                    bufctr = 0;
                    bufptr = buf;
                } else {
                    bufptr += packet_size;
                }
                last_packet_num=packet_num;

                /* Disk stuff */
                if (disk_out && ((bufctr==0) || (bufctr==buffer_size/2)) ){
                    /* Wait for last write thread to finish */
                    if (!first_write) { pthread_join(write_tid, NULL); }
                    /* Launch new write thread */
                    if (bufctr==0) { out.buf = buf+buffer_size_bytes/2; }
                    else { out.buf = buf; }
                    rv = pthread_create(&write_tid, NULL, write_to_disk,
                            (void *)&out);
                    if (rv) { 
                        perror("pthread_create");
                        exit(1);
                    }
                    first_write=0;
                }

                /* Check if we need to stop */
                if ((npacket>0) && (packet_count>=npacket)) run = 0;

            }
        } else if (rv==0) {
            if (first==0) { run=0; timeout=1; } /* No data for 1 sec => quit */
        } else {
            if (errno==EINTR) { run=0; }
            else {
                perror("poll");
                exit(1);
            }
        }
    }
    // wait for write thread
    if (disk_out) {
        pthread_join(write_tid,NULL);
        //close(out.fd);
        fclose(out.fd);
    }
    time1 = times(&t1);

    sent_count -= packet_0;

    double time_sec = (double)(time1-time0)/(double)tps;
    if (timeout && (poll_timeout>0)) {
        time_sec -= (double)poll_timeout/1000.0;
    }
    double load = 
        (double)(t1.tms_utime+t1.tms_stime-t0.tms_utime-t0.tms_stime) /
        (double)(time1-time0);
    double rate = (double)byte_count/time_sec;
    double srate = (double)sent_count * (double)packet_size / time_sec;

    drop_count = sent_count - packet_count;

    if (quiet) {
        printf("%5d %8.1f %8.3f %.3e %5.3f %d %d R:%s\n",
                packet_size, byte_count/(1024.0*1024.0), 
                rate/(1024.0*1024.0), (double)drop_count/(double)sent_count,
                load, drop_count, cpu_idx, argv[optind]);
    } else {
        printf("Receiving from %s\n", argv[optind]);
        printf("Packet size %d B\n", packet_size);
        printf("Got %.1f MB\n", byte_count/(1024.0*1024.0)); 
        printf("Recv rate %.3f MB/s\n", rate/(1024.0*1024.0));
        printf("Send rate %.3f MB/s\n", srate/(1024.0*1024.0));
        printf("Dropped %d packets\n", drop_count);
        printf("Drop rate %.3e\n", (double)drop_count/(double)sent_count);
        printf("Avg load %.3f\n", load);
    }

    exit(0);

}
