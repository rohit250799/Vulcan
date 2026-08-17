#include <csignal>
#include <cstddef>
#include <cstdint>
#include <linux/if_packet.h>
#include <memory>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/uio.h>

#pragma once

namespace vulcan::feed {

struct ring {
  std::unique_ptr<struct iovec[]> rd;
  uint8_t *map;
  struct tpacket_req3 req;
};

struct block_desc {
  uint32_t version;
  uint32_t offset_to_priv;
  struct tpacket_hdr_v1 h1;
};

void get_socket_option_or_die(int sockfd, void *optval, socklen_t *optlen,
                              int level = SOL_PACKET,
                              int optname = PACKET_STATISTICS);

class Zero_Copy_UDP_Listener {
private:
  int sockfd{-1};
  void *mmap_rx_ring;
  std::size_t ring_size;
  int target_core;

  void set_cpu_affinity();    // pinning thread to prevent OS from migrating it
  void enable_busy_polling(); // agressive driver polling to bypass interrupts

public:
  long packets_total = 0, bytes_total = 0;
  static volatile std::sig_atomic_t sigint;
  // Zero_Copy_UDP_Listener(int core_id);
  Zero_Copy_UDP_Listener();
  ~Zero_Copy_UDP_Listener();
  [[nodiscard]] int get_sockfd() const noexcept { return sockfd; }

  void init_socket(); // instantiating raw_socket
  int setup_mmap_ring(
      struct ring *ring); // Map the NIC Rx queue directly to userspace
  static void sighandler(int num);
  void display(struct tpacket3_hdr *ppd);
  void walk_block(struct block_desc *pbd, const int block_num);
  void flush_block(struct block_desc *pbd);
  void poll_loop(); // critical hot-path, no syscalls allowed here
  void test_UDP_ping_pong_with_jitter();
};

} // namespace vulcan::feed