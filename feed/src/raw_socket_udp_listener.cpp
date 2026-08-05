#include <asm-generic/socket.h>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <linux/if_packet.h>
#include <netinet/in.h>
#include <poll.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>
#include <iostream>
#include "vulcan/feed/raw_socket_udp_listener.hpp"
#include "vulcan/core/Attributes.h"
#include "vulcan/core/Errorcode.h"
#include "vulcan/core/Fatal.h"

namespace vulcan::feed {

VULCAN_COLD VULCAN_NOINLINE void
handle_socket_fatal(vulcan::core::ErrorCode code, int saved_errno) noexcept {
  vulcan::core::fatal(code, saved_errno);
  return;
}

void bind_or_die(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  if (bind(sockfd, addr, addrlen) < 0) {
    int saved_errno = errno;
    handle_socket_fatal(vulcan::core::ErrorCode::ConnectionLost, saved_errno);
  }
  return;
}

//} // namespace vulcan::feed

#define SERV_PORT 8080

void dg_echo(int sockfd, sockaddr *pcliaddr,
                                     socklen_t clilen) {
  int n;
  socklen_t len;
  const int MAXLINE = 1024;
  char my_message[MAXLINE];

  for (;;) { // simple loop to read the next datagram arriving at the server
             // port using recvfrom and send it back using sendto - iterative
             // server, not concurrent
    len = clilen;
    n = recvfrom(sockfd, my_message, MAXLINE, 0, pcliaddr, &len);
    if (n == -1)
      continue; // ignoring failed request
    size_t sendto_result = sendto(sockfd, my_message, n, 0, pcliaddr, len);
    if (sendto_result != n) 
        handle_socket_fatal(vulcan::core::ErrorCode::ConnectionLost, errno);
  }
  return;
}

void dg_cli(FILE *fp, int sockfd, const sockaddr *pservaddr, socklen_t servlen) {
    int n;
    const int MAXLINE = 1024;
    char sendline[MAXLINE], recvline[MAXLINE + 1];
    
    while (fgets(sendline, MAXLINE, fp) != NULL) {
        ssize_t expected_len = static_cast<ssize_t>(strlen(sendline));
        ssize_t sent = sendto(sockfd, sendline, expected_len, 0, pservaddr, servlen);
        
        if (VULCAN_UNLIKELY(sent != expected_len)) {
            int saved_errno = errno;
            handle_socket_fatal(vulcan::core::ErrorCode::ConnectionLost, saved_errno);
        }
        
        ssize_t n = recvfrom(sockfd, recvline, MAXLINE, 0, NULL, NULL);
        if (VULCAN_UNLIKELY(n == -1)) {
            int saved_errno = errno;
            handle_socket_fatal(vulcan::core::ErrorCode::ConnectionLost, saved_errno);
        }
        
        recvline[n] = '\0'; // null terminate
        if (VULCAN_UNLIKELY(fputs(recvline, stdout) == EOF)) {
            int saved_errno = errno;
            handle_socket_fatal(vulcan::core::ErrorCode::ResourceExhausted, saved_errno);
        }
    }
    return;
}

Zero_Copy_UDP_Listener::Zero_Copy_UDP_Listener() {
  std::cout << "Setting up a zero copy UDP listener";
}

// Zero_Copy_UDP_Listener::init_socket() {
//     sockfd = socket(AF_PACKET, SOCK_RAW, IPROTO_UDP);
//     if (sockfd < 0) {
//         perror("socket");
//         exit(1);
//     }
//     setup_mmap_ring();
//     return;
// }

// void Zero_Copy_UDP_Listener::setup_mmap_ring() {
//     const size_t memory_space = 16000000;
//     struct tpacket_req req = {4096, 4, 2048, 8};
//     setsockopt(sockfd, SOL_PACKET, PACKET_RX_RING, (void*) &req,
//     sizeof(req)); mmap_rx_ring = mmap(0, memory_space, PROT_READ |
//     PROT_WRITE, MAP_SHARED, sockfd, 0); if (mmap_rx_ring == MAP_FAILED)
//         err(EXIT_FAILURE, "mmap");
//     return;
// }

// void Zero_Copy_UDP_Listener::poll_loop() {
//     struct pollfd *pfds;
//     auto pfds = std::make_unique<pollfd[]>(10);
//     if (pfds == NULL)
//         err(EXIT_FAILURE, "malloc");

//     pfds.fd = sockfd;
//     if (pfds.fd == -1)
//         err(EXIT_FAILURE, "open");
//     //pfds.events = POLLIN; // this is to be enabled once there is data
//     available to read, else its useless now and results in logical errors
// }

void Zero_Copy_UDP_Listener::test_UDP_ping_pong_with_jitter() {
  struct sockaddr_in servaddr, cliaddr;
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(SERV_PORT);

  bind_or_die(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

  dg_echo(sockfd, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
  dg_cli(stdin, sockfd, (struct sockaddr*)&cliaddr, sizeof(servaddr));
}
}