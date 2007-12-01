/* r_net.c */
/* Learn how to recv tcp packets */

/* There are two sockets involved.  One listens for connections,
 * and needs to be bound to a specific port.  When a connection
 * comes in, accept assigns it to a second socket.
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
// #include <netinet/in.h>
#include <linux/in.h>
#include <sys/times.h>
#include <errno.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int sock_num, sock_num_c;
  int retval;
  int run=1;
  int mtu_size, dum=sizeof(mtu_size);
  char *data;
  clock_t time_0, time_1;
  char c;
  int a;
  double rate;
  long long int n = 0;
  struct sockaddr_in ip_address, ip_address_c;
  long int tps = sysconf(_SC_CLK_TCK);

  // Open socket.  "6" means TCP. or is it "0"?
  // udp -> use SOCK_DGRAM
  // tcp -> use SOCK_STREAM
  sock_num = socket(PF_INET, SOCK_STREAM, 0);
  if (sock_num == -1) { perror("socket"); exit(1); }

  // Create ip address structure
  ip_address.sin_family = AF_INET;
  ip_address.sin_port = 5000;
  retval = inet_aton(argv[1], &ip_address.sin_addr);
  if (retval == 0) { fprintf(stderr, "Bad IP addr.\n"); exit(1); }

  // assign ("bind") specific ip addr, port to this socket.
  retval = bind(sock_num, (struct sockaddr *)&ip_address, sizeof(ip_address));
  if (retval == -1) { perror("bind"); exit(1); }

  // tell the socket to listen for incoming connections.
  retval = listen(sock_num, 2);
  if (retval == -1) { perror("listen"); exit(1); }

  printf("Waiting for a connection.\n");

  // wait for a connection (blocking mode - accept hangs until one comes)
  a = sizeof(ip_address_c);
  sock_num_c = accept(sock_num, (struct sockaddr *)&ip_address_c, &a);
  if (sock_num_c == -1) { perror("accept"); exit(1); }

  // Close down the "listening" socket now.
  retval = shutdown(sock_num, 2);
  if (retval == -1) { perror("shutdown"); exit(1); }
  retval = close(sock_num);
  if (retval == -1) { perror("close"); exit(1); }

  printf("Connected ok!\n");

//  getsockopt(sock_num, 0, IP_MTU, &mtu_size, &dum);
//  printf("IP_MTU = %d\n", mtu_size);

  mtu_size = 32768;

  data = (char *)malloc(sizeof(char) * mtu_size);

/*
  // Talk to other end here!
  while (run) {
    retval = read(sock_num_c, &c, 1);
    if (retval == -1) { perror("read"); run = 0; }
    else if (c == EOF) { run = 0; }
    else {
      putchar((int)c);
    }
  }
*/

  time_0 = times(NULL);
  while (run) {
    retval = read(sock_num_c, data, mtu_size);
    if (retval == -1) { perror("read"); run = 0; }
    if (retval == 0) { fprintf(stderr, "Read 0 bytes.\n"); run = 0; }
    if (run == 0) break;
    n += retval;
  }
  time_1 = times(NULL);

//  getsockopt(sock_num, 0, IP_MTU, &mtu_size, &dum);
//  printf("IP_MTU = %d\n", mtu_size);

  rate = (double) (time_1 - time_0) / tps;
  printf("Time = %f s\n", rate);
  rate = (double) (n >> 20) / rate;
  printf("N_bytes = %dM\n", n>>20);
  printf("Rate = %f MB/s\n", rate);

  retval = shutdown(sock_num_c, 2);
  if (retval == -1) { perror("shutdown"); exit(1); }

  retval = close(sock_num_c);
  if (retval == -1) { perror("close"); exit(1); }

  return(0);

}

