/* s_net.c */
/* Learn how to send tcp packets */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
// #include <netinet/in.h>
#include <sys/times.h>
#include <linux/in.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#define N_LOOP 6000

int run = 1;
void cc(int sig) {
  // Handle control-c
  run = 0;
}

int main(int argc, char *argv[]) {
  int i;
  int sock_num;
  int retval;
  int mtu_size, dum=sizeof(mtu_size);
  char *data;
  char c;
  double rate = 0;
  unsigned long int n = 0;
  struct sockaddr_in ip_address;
  clock_t time_0, time_1;
  long int tps = sysconf(_SC_CLK_TCK);
  int n_loop = N_LOOP;

  n_loop = 32*1024;

  signal(SIGINT, cc);

  // Open socket.  "6" means TCP. or is it "0"? what about UDP?
  // udp -> use SOCK_DGRAM
  // tcp -> use SOCK_STREAM
  sock_num = socket(PF_INET, SOCK_STREAM, 0);
  if (sock_num == -1) { perror("socket"); exit(1); }

  // create ip address struct
  ip_address.sin_family = AF_INET;
  ip_address.sin_port = 5000;
  retval = inet_aton(argv[1], &ip_address.sin_addr);
  if (retval == 0) { fprintf(stderr, "Bad IP addr.\n"); exit(1); }

  // connect via specific ip addr, port
  retval = connect(sock_num, (struct sockaddr *)&ip_address, 
                     sizeof(ip_address));
  if (retval == -1) { perror("connect"); exit(1); }

  printf("Connected ok!\n");

  // check MTU - "Maximum Transmission Unit"
  getsockopt(sock_num, 0, IP_MTU, &mtu_size, &dum);
  printf("IP_MTU = %d\n", mtu_size);

  mtu_size=32768;

  data = (char *)malloc(sizeof(char) * mtu_size);

  // Talk to other end in here!
/*  while (run) {
    c = (char) getchar();
    if (c == EOF) { run = 0; }
    retval = write(sock_num, &c, 1);
    if (retval == -1) { perror("write"); run = 0; }
  }
*/

  time_0 = times(NULL);
  for (i=0; i<n_loop; i++) {
    retval = write(sock_num, data, mtu_size);
    if (retval == -1) { perror("write"); run = 0; }
    if (run == 0) break;
    n += retval;
  }
  time_1 = times(NULL);

  rate = (double)(time_1 - time_0)/tps;
  printf("Time = %f s\n", rate);
  printf("N_bytes = %dM\n", n>>20);
  rate = (double)(n>>20) / rate;
  printf("Rate = %f MB/s\n", rate);

  // close socket.
  retval = close(sock_num);
  if (retval == -1) { perror("close"); exit(1); }

  return(0);

}

