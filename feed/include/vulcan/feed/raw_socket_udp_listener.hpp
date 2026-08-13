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

class Zero_Copy_UDP_Listener {
private:
  int sockfd;
  void *mmap_rx_ring;
  std::size_t ring_size;
  int target_core;

  void set_cpu_affinity();    // pinning thread to prevent OS from migrating it
  void enable_busy_polling(); // agressive driver polling to bypass interrupts

public:
  // Zero_Copy_UDP_Listener(int core_id);
  Zero_Copy_UDP_Listener();
  ~Zero_Copy_UDP_Listener();

  void init_socket(); // instantiating raw_socket
  int setup_mmap_ring(
      struct ring *ring); // Map the NIC Rx queue directly to userspace
  void poll_loop(); // critical hot-path, no syscalls allowed here
  void test_UDP_ping_pong_with_jitter();
  
};

} // namespace vulcan::feed