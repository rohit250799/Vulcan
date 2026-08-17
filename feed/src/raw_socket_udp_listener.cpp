#include "vulcan/feed/raw_socket_udp_listener.hpp"
#include "vulcan/core/Attributes.h"
#include "vulcan/core/Errorcode.h"
#include "vulcan/core/Fatal.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <iostream>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <memory>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
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
  // int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  //  int sockfd = -1;
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

void *mmap_or_die(int sockfd, size_t map_length,
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

void set_socket_option_or_die(int sockfd, void *optval, socklen_t optlen,
                              int level = SOL_PACKET,
                              int optname = PACKET_VERSION) {
  assert(sockfd != -1 && "Assertion failed, sockfd is -1 \n");
  int set_socket_option_result =
      setsockopt(sockfd, level, optname, optval, optlen);
  if (set_socket_option_result < 0)
    handle_socket_fatal(vulcan::core::ErrorCode::ResourceAcquisitionFailed,
                        errno);
  return;
}

void get_socket_option_or_die(int sockfd, void *optval, socklen_t *optlen,
                              int level, int optname) {
  assert(sockfd != -1 && "Assertion failed, sockfd is -1 \n");
  int get_socket_option_result =
      getsockopt(sockfd, level, optname, optval, optlen);
  if (get_socket_option_result < 0)
    handle_socket_fatal(vulcan::core::ErrorCode::ResourceAcquisitionFailed,
                        errno);
  return;
}

#define SERV_PORT 8080
volatile std::sig_atomic_t vulcan::feed::Zero_Copy_UDP_Listener::sigint = 0;

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
}

Zero_Copy_UDP_Listener::~Zero_Copy_UDP_Listener() { close(sockfd); }

int Zero_Copy_UDP_Listener::setup_mmap_ring(struct ring *ring) {
  unsigned int blocksiz = 1 << 22,
               framesiz = 1 << 11; // block size will be 4 mb in this case
  unsigned int blocknum = 64;
  memset(&ring->req, 0, sizeof(ring->req));
  ring->req.tp_block_size = blocksiz;
  ring->req.tp_frame_size = framesiz;
  ring->req.tp_block_nr = blocknum;
  ring->req.tp_frame_nr = (blocksiz * blocknum) / framesiz;
  ring->req.tp_retire_blk_tov = 60;
  ring->req.tp_feature_req_word = TP_FT_REQ_FILL_RXHASH;
  set_socket_option_or_die(sockfd, &ring->req, sizeof(ring->req), SOL_PACKET,
                           PACKET_RX_RING);

  struct sockaddr_ll ll;
  ring->map = (uint8_t *)mmap_or_die(
      sockfd, ring->req.tp_block_size * ring->req.tp_block_nr,
      PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED);
  ring->rd = std::make_unique<struct iovec[]>(
      ring->req.tp_block_nr); // need to allocate certain bytes of memory (block
                              // numbers * size of rd struct), example shown in
                              // my_project_notes.md
  assert(ring->rd);
  for (int i = 0; i < ring->req.tp_block_nr; ++i) {
    ring->rd[i].iov_base = ring->map + (i * ring->req.tp_block_size);
    ring->rd[i].iov_len = ring->req.tp_block_size;
  }

  memset(&ll, 0, sizeof(ll));
  ll.sll_family = AF_PACKET;
  ll.sll_protocol = htons(ETH_P_ALL);
  ll.sll_ifindex = if_nametoindex(
      "eno1"); // since I am using the Ethernet port for this project
  ll.sll_hatype = 0;
  ll.sll_pkttype = 0;
  ll.sll_halen = 0;

  bind_or_die(sockfd, (struct sockaddr *)&ll, sizeof(ll));
  std::cout << "Ring has been setup \n";
  return sockfd;
}

void Zero_Copy_UDP_Listener::sighandler(int num) { sigint = 1; }

void Zero_Copy_UDP_Listener::display(struct tpacket3_hdr *ppd) {
  struct ethhdr *eth =
      (struct ethhdr *)((uint8_t *)ppd +
                        ppd->tp_mac); // tp_mac is the offset from header start
                                      // to MAC/Ethernet layer
  struct iphdr *ip =
      (struct iphdr *)((uint8_t *)eth + ETH_HLEN); // ipv4 packet header

  if (eth->h_proto == htons(ETH_P_IP)) {
    struct sockaddr_in ss, sd;
    char sbuff[NI_MAXHOST], dbuff[NI_MAXHOST];

    memset(&ss, 0, sizeof(ss));
    ss.sin_family = AF_INET;
    ss.sin_addr.s_addr = ip->saddr; // points to the source ip address
    getnameinfo((struct sockaddr *)&ss, sizeof(ss), sbuff, sizeof(sbuff), NULL,
                0, NI_NUMERICHOST);

    memset(&sd, 0, sizeof(sd));
    sd.sin_family = AF_INET;
    sd.sin_addr.s_addr = ip->daddr;
    getnameinfo((struct sockaddr *)&sd, sizeof(sd), dbuff, sizeof(dbuff), NULL,
                0, NI_NUMERICHOST);
    printf("%s -> %s\n", sbuff, dbuff);
  }
  printf("rxhash: 0x%x\n", ppd->hv1.tp_rxhash);
  return;
}

void Zero_Copy_UDP_Listener::walk_block(struct block_desc *pbd,
                                        const int block_num) {
  int num_pkts = pbd->h1.num_pkts, i;
  unsigned long bytes = 0;
  struct tpacket3_hdr *ppd;

  ppd = (struct tpacket3_hdr *)((uint8_t *)pbd + pbd->h1.offset_to_first_pkt);
  for (i = 0; i < num_pkts; ++i) {
    bytes += ppd->tp_snaplen;
    display(ppd);
    ppd = (struct tpacket3_hdr *)((uint8_t *)ppd + ppd->tp_next_offset);
  }

  packets_total += num_pkts;
  bytes_total += bytes;
  return;
}

void Zero_Copy_UDP_Listener::flush_block(struct block_desc *pbd) {
  pbd->h1.block_status = TP_STATUS_KERNEL;
}

void Zero_Copy_UDP_Listener::test_UDP_ping_pong_with_jitter() {
  struct sockaddr_in servaddr, cliaddr;
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(SERV_PORT);

  bind_or_die(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

  // dg_echo(sockfd, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
  // dg_cli(stdin, sockfd, (struct sockaddr *)&cliaddr, sizeof(servaddr));
}
} // namespace vulcan::feed
