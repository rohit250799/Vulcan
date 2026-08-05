#define _GNU_SOURCE

#include <array>
#include <strings.h>

#include <arpa/inet.h>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cpuid.h>
#include <cstddef>
#include <cstdlib>
#include <emmintrin.h>
#include <functional>
#include <immintrin.h>
#include <iomanip>
#include <iostream>
#include <netinet/in.h>
#include <pthread.h>
#include <sched.h>
#include <sys/socket.h>
#include <thread>
#include <type_traits>

#include "vulcan/lock_free_spsc_queue.hpp"
#include "vulcan/queue_core.hpp"
#include "vulcan/raw_socket_udp_listener.hpp"

#define SERV_PORT 8080

void push_orders_to_queue(LockFreeSPSCQueue<QueueOrder, 256> &queue,
                          std::atomic<bool> &m_start_ref, int core_id) {
  alignas(64) std::array<QueueOrder, 8> queue_orders;
  populate_stack_buffer(queue_orders);
  pthread_t current_thread_id = pthread_self();
  pinThreadToCore(current_thread_id, core_id);
  assert(sched_getcpu() == core_id &&
         "Assertion failed: Pinning is not working \n");

  while (!m_start_ref.load(std::memory_order_acquire)) {
    _mm_pause(); // notify LSU that the thread is in a spin-wait, reducing power
                 // consumption and preventing pipeline from being corrupted
                 // with speculative loads
  }

  size_t local_current_tail = 0;
  size_t local_current_head_cached = 0;

  unsigned int eax, ebx, ecx, edx;
  __cpuid(0, eax, ebx, ecx, edx);

  const int TOTAL_BURST_ORDERS = 1000000000;

  for (size_t i = 0; i < TOTAL_BURST_ORDERS; i += 8) {
    uint64_t next_local_tail = (local_current_tail + 8) & 255;
    if (next_local_tail == local_current_head_cached) {
      while (next_local_tail == local_current_head_cached) {
        _mm_pause();
        local_current_head_cached = queue.load_head_acquire();
      }
    }
    queue.producer_uncommitted_push(queue_orders[0], local_current_tail);
    local_current_tail = next_local_tail;
    local_current_tail = (local_current_tail + 1) & 255;
    queue.producer_uncommitted_push(queue_orders[1], local_current_tail);
    local_current_tail = (local_current_tail + 1) & 255;
    queue.producer_uncommitted_push(queue_orders[2], local_current_tail);
    local_current_tail = (local_current_tail + 1) & 255;
    queue.producer_uncommitted_push(queue_orders[3], local_current_tail);
    local_current_tail = (local_current_tail + 1) & 255;
    queue.producer_uncommitted_push(queue_orders[4], local_current_tail);
    local_current_tail = (local_current_tail + 1) & 255;
    queue.producer_uncommitted_push(queue_orders[5], local_current_tail);
    local_current_tail = (local_current_tail + 1) & 255;
    queue.producer_uncommitted_push(queue_orders[6], local_current_tail);
    local_current_tail = (local_current_tail + 1) & 255;
    queue.producer_uncommitted_push(queue_orders[7], local_current_tail);
    local_current_tail = (local_current_tail + 1) & 255;

    queue.publish_tail_release(local_current_tail);
  }
  queue.publish_tail_release(local_current_tail);
  _mm_lfence();
  std::cout << "Producer thread is warmed up for now.. \n";
  return;
}

void pop_orders_from_queue(LockFreeSPSCQueue<QueueOrder, 256> &queue,
                           std::atomic<bool> &m_start_ref, int core_id) {
  pthread_t current_thread_id = pthread_self();
  pinThreadToCore(current_thread_id, core_id);
  assert(sched_getcpu() == core_id &&
         "Assertion failed: Pinning is not working in consumer thread \n");

  size_t local_head_index = 0;
  size_t local_tail_cached_test = 0;

  while (!m_start_ref.load(std::memory_order_acquire)) {
    _mm_pause(); // same as in push_orders_to_queue function (waiting for thread
                 // to finish OS-level init, pinning to cores, already spinning
                 // and hot in their L1i)
  }

  const int TOTAL_BURST_ORDERS = 1000000000;
  uint64_t local_head_idx =
      0; // tracks head location of queue for reading the incoming orders
  uint64_t local_tail_cached = 0;
  size_t batch_count = 0;
  double local_register_accumulator_0, local_register_accumulator_1,
      local_register_accumulator_2, local_register_accumulator_3,
      local_register_accumulator_4, local_register_accumulator_5,
      local_register_accumulator_6, local_register_accumulator_7 = 0.0;

  for (size_t i = 0; i < TOTAL_BURST_ORDERS; i += 8) {
    if (local_head_idx == local_tail_cached) {
      while (local_head_idx == local_tail_cached) {
        _mm_pause();
        local_tail_cached = queue.load_tail_acquire();
      }
    }
    const QueueOrder *current_head_index_ptr_0 =
        queue.consumer_uncommitted_peek(local_head_idx);
    const QueueOrder *current_head_index_ptr_1 =
        queue.consumer_uncommitted_peek(local_head_idx + 1);
    const QueueOrder *current_head_index_ptr_2 =
        queue.consumer_uncommitted_peek(local_head_idx + 2);
    const QueueOrder *current_head_index_ptr_3 =
        queue.consumer_uncommitted_peek(local_head_idx + 3);
    const QueueOrder *current_head_index_ptr_4 =
        queue.consumer_uncommitted_peek(local_head_idx + 4);
    const QueueOrder *current_head_index_ptr_5 =
        queue.consumer_uncommitted_peek(local_head_idx + 5);
    const QueueOrder *current_head_index_ptr_6 =
        queue.consumer_uncommitted_peek(local_head_idx + 6);
    const QueueOrder *current_head_index_ptr_7 =
        queue.consumer_uncommitted_peek(local_head_idx + 7);

    local_register_accumulator_0 += current_head_index_ptr_0->price;
    local_register_accumulator_1 += current_head_index_ptr_1->price;
    local_register_accumulator_2 += current_head_index_ptr_2->price;
    local_register_accumulator_3 += current_head_index_ptr_3->price;
    local_register_accumulator_4 += current_head_index_ptr_4->price;
    local_register_accumulator_5 += current_head_index_ptr_5->price;
    local_register_accumulator_6 += current_head_index_ptr_6->price;
    local_register_accumulator_7 += current_head_index_ptr_7->price;

    local_head_idx = (local_head_idx + 8) & 255;
    batch_count++;
    if ((batch_count & 7) == 0) {
      queue.publish_head_release(local_head_idx);
    }
  }
  queue.publish_head_release(local_head_idx);
  auto total_accumulator_sum =
      ((local_register_accumulator_0 + local_register_accumulator_1) +
       (local_register_accumulator_2 + local_register_accumulator_3) +
       (local_register_accumulator_4 + local_register_accumulator_5) +
       (local_register_accumulator_6 + local_register_accumulator_7));
  _mm_lfence();
  std::cout << "Consumer thread is warmed up right now.. \n";
  return;
}

int main() {
  int core1 = 0;
  int core2 = 2;
  std::atomic<bool> m_start(false);

  LockFreeSPSCQueue<QueueOrder, 256> *my_queue =
      LockFreeSPSCQueue<QueueOrder, 256>::create();
  std::thread producer_thread(push_orders_to_queue, std::ref(*my_queue),
                              std::ref(m_start), core1);
  std::thread consumer_thread(pop_orders_from_queue, std::ref(*my_queue),
                              std::ref(m_start), core2);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  m_start.store(true, std::memory_order_release);

  producer_thread.join();
  consumer_thread.join();

  Zero_Copy_UDP_Listener my_listener;
  my_listener.test_UDP_ping_pong_with_jitter();

  // int sockfd;
  // struct sockaddr_in servaddr;
  // bzero(&servaddr, sizeof(servaddr));
  // servaddr.sin_family = AF_INET;
  // servaddr.sin_port = htons(SERV_PORT);
  // if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0)
  //     err(EXIT_FAILURE, "Connection error");

  // sockfd =

  return 0;
}
