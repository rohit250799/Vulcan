#include <cstddef>
#include <sys/socket.h>

class Zero_Copy_UDP_Listener {
private:
  int sockfd;
  void *mmap_rx_ring;
  std::size_t ring_size;
  int target_core;

  void set_cpu_affinity();    // pinning thread to prevent OS from migrating it
  void enable_busy_polling(); // agressive driver polling to bypass interrupts
  void setup_mmap_ring();     // Map the NIC Rx queue directly to userspace
  void dg_echo(int sockfd, sockaddr *pcliaddr, socklen_t clilen);

public:
  // Zero_Copy_UDP_Listener(int core_id);
  Zero_Copy_UDP_Listener();
  ~Zero_Copy_UDP_Listener() = default;

  void init_socket(); // instantiating raw_socket

  void poll_loop(); // critical hot-path, no syscalls allowed here
  void test_UDP_ping_pong_with_jitter();
};
