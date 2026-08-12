#include "vulcan/feed/raw_socket_udp_listener.hpp"
#include "vulcan/core/Attributes.h"
#include "vulcan/core/Errorcode.h"
#include "vulcan/core/Fatal.h"
#include <asm-generic/socket.h>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <iostream>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <netinet/in.h>
#include <poll.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <unistd.h>

namespace vulcan::feed {

VULCAN_COLD VULCAN_NOINLINE void
handle_socket_fatal(vulcan::core::ErrorCode code, int saved_errno) noexcept {
  vulcan::core::fatal(code, saved_errno);
  return;
}

int create_capture_socket_or_die() {
    //int sockfd = -1;
    int sockfd = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_ALL));
    if (sockfd == -1)
        handle_socket_fatal(vulcan::core::ErrorCode::ConnectionLost, errno);
    return sockfd;
}

void bind_or_die(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  if (bind(sockfd, addr, addrlen) < 0) {
    int saved_errno = errno;
    handle_socket_fatal(vulcan::core::ErrorCode::ConnectionLost, saved_errno);
  }
  return;
}

void* mmap_or_die(int sockfd, size_t map_length,
                  int prot = PROT_READ | PROT_WRITE, int flags = MAP_SHARED,
                  off_t offset = 0) {
  void *mapped_buffer = mmap(0, map_length, prot, flags, sockfd, offset);
  if (mapped_buffer == MAP_FAILED)
    handle_socket_fatal(vulcan::core::ErrorCode::MemoryMappingFailed, errno);
  return mapped_buffer;
}

void handle_polling_error(int polling_returned_value) {
    if (polling_returned_value == 1)
        handle_socket_fatal(vulcan::core::ErrorCode::ConnectionLost, errno);
    if (polling_returned_value == 0)
        handle_socket_fatal(vulcan::core::ErrorCode::Timeout, errno);
    return;
}

void set_socket_option_or_die(int sockfd, void* optval, socklen_t optlen, int level=SOL_PACKET, int optname=PACKET_VERSION) {
  assert(sockfd != -1 && "Assertion failed, sockfd is -1 \n");
  int set_socket_option_result =
      setsockopt(sockfd, level, optname, optval, optlen);
  if (set_socket_option_result < 0)
      handle_socket_fatal(vulcan::core::ErrorCode::ResourceAcquisitionFailed,
                        errno);
  return;
}

#define SERV_PORT 8080

void dg_echo(int sockfd, sockaddr *pcliaddr, socklen_t clilen) {
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

void dg_cli(FILE *fp, int sockfd, const sockaddr *pservaddr,
            socklen_t servlen) {
  //int n;
  const int MAXLINE = 1024;
  char sendline[MAXLINE], recvline[MAXLINE + 1];

  while (fgets(sendline, MAXLINE, fp) != NULL) {
    ssize_t expected_len = static_cast<ssize_t>(strlen(sendline));
    ssize_t sent =
        sendto(sockfd, sendline, expected_len, 0, pservaddr, servlen);

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
      handle_socket_fatal(vulcan::core::ErrorCode::ResourceExhausted,
                          saved_errno);
    }
  }
  return;
}

Zero_Copy_UDP_Listener::Zero_Copy_UDP_Listener() {
  std::cout << "Setting up a zero copy UDP listener \n";
  sockfd = -1;
  int version = TPACKET_V3;
  sockfd = create_capture_socket_or_die();
  assert(sockfd != -1 && "Assertion failed: Socket creation returned -1\n");
  set_socket_option_or_die(sockfd, &version, sizeof(version));
  //void *mapped_buffer = mmap_or_die(sockfd, mmap_length); // for mapping of allocated buffer to user process
  //setup_mmap_ring();
}

Zero_Copy_UDP_Listener::~Zero_Copy_UDP_Listener() { close(sockfd); }

void Zero_Copy_UDP_Listener::setup_mmap_ring(struct ring* ring) {
    unsigned int blocksiz = 1 << 22, framesiz = 1 << 11; // block size will be 4 mb in this case
    unsigned int blocknum = 64;
    memset(&ring->req, 0, sizeof(ring->req));
    ring->req.tp_block_size = blocksiz;
    ring->req.tp_frame_size = framesiz;
    ring->req.tp_block_nr = blocknum;
    ring->req.tp_frame_nr = (blocksiz * blocknum) / framesiz;
    ring->req.tp_retire_blk_tov = 60;
    ring->req.tp_feature_req_word = TP_FT_REQ_FILL_RXHASH;
    size_t mmap_length = 100;
    // ring_size = 100 * 2;
    // mmap_rx_ring = mmap_or_die(0, ring_size);
    return;
}

void Zero_Copy_UDP_Listener::poll_loop() {
  struct pollfd pfd;
  struct tpacket_hdr* ps_header;
  pfd.fd = sockfd;
  pfd.revents = 0;
  pfd.events = POLLIN | POLLRDNORM | POLLERR;

  if (ps_header->tp_status == TP_STATUS_KERNEL) {
       int retval = poll(&pfd, 1, 0);
       handle_polling_error(retval);
  }
  return;
}

void Zero_Copy_UDP_Listener::test_UDP_ping_pong_with_jitter() {
  struct sockaddr_in servaddr, cliaddr;
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(SERV_PORT);

  bind_or_die(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

  dg_echo(sockfd, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
  dg_cli(stdin, sockfd, (struct sockaddr *)&cliaddr, sizeof(servaddr));
}
} // namespace vulcan::feed
