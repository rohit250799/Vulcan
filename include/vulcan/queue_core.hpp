#include <array>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <emmintrin.h>
#include <iostream>
#include <pthread.h>

const size_t TOTAL_WARMUP_OPS = 1000000;

void pinThreadToCore(pthread_t thread_id, int core_id) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);

  int set_affinity_mask_result =
      pthread_setaffinity_np(thread_id, sizeof(cpu_set_t), &cpuset);
  if (set_affinity_mask_result != 0) {
    std::cerr << "Error calling pthread_setaffinity_np for core: " << core_id
              << "has error: " << EXIT_FAILURE << " \n";
  }
  // Ensuring the thread is allowed to run on the target core
  bool is_set = CPU_ISSET(core_id, &cpuset);
  // Ensuring thread is ONLY pinned to this core (affinity count == 1)
  int total_cores_in_mask = CPU_COUNT(&cpuset);
  assert(is_set && total_cores_in_mask == 1 &&
         "Thread is not strictly pinned to the target core!");
}

bool populate_stack_buffer(std::array<QueueOrder, 8> &my_queueOrder) {
  for (size_t i = 0; i < 8; ++i) {
    my_queueOrder[i] = {i, (i + 1) * 1.33, i + 100};
  }
  assert(my_queueOrder[7].order_id == 7 &&
         "Assertion failed: Stack buffer not populated correctly \n");
  return true;
}

void warm_up_producer(LockFreeSPSCQueue<QueueOrder, 256> &lock_free_queue_ref,
                      std::array<QueueOrder, 8> &queue_orders_ref) {
  size_t local_current_tail = 0;
  size_t local_current_head_cached = 0;
  populate_stack_buffer(queue_orders_ref);
  uint64_t batch_count = 0;
  for (size_t i = 0; i < TOTAL_WARMUP_OPS; ++i) {
    size_t next_local_tail = (local_current_tail + 1) & 255;
    if (next_local_tail == local_current_head_cached) {
      while (next_local_tail == local_current_head_cached) {
        _mm_pause();
        local_current_head_cached = lock_free_queue_ref.load_head_acquire();
      }
    }
    size_t buffer_index = local_current_tail & 7;
    lock_free_queue_ref.producer_uncommitted_push(
        queue_orders_ref[buffer_index], local_current_tail);
    local_current_tail = next_local_tail;
    batch_count++;
    if ((batch_count & 7) == 0) {
      lock_free_queue_ref.publish_tail_release(local_current_tail);
    }
  }
  lock_free_queue_ref.publish_tail_release(local_current_tail);
  return;
}

void warm_up_consumer(LockFreeSPSCQueue<QueueOrder, 256> &lock_free_queue_ref) {
  size_t local_head_idx = 0;
  uint64_t local_tail_cached = 0;
  uint64_t batch_count = 0;
  double local_register_accumulator_0, local_register_accumulator_1,
      local_register_accumulator_2, local_register_accumulator_3,
      local_register_accumulator_4, local_register_accumulator_5,
      local_register_accumulator_6, local_register_accumulator_7 = 0.0;

  for (size_t i = 0; i < TOTAL_WARMUP_OPS; ++i) {
    if (local_head_idx == local_tail_cached) {
      while (local_head_idx == local_tail_cached) {
        _mm_pause();
        local_tail_cached = lock_free_queue_ref.load_tail_acquire();
      }
    }

    const QueueOrder *current_head_index_ptr_0 =
        lock_free_queue_ref.consumer_uncommitted_peek(local_head_idx);
    const QueueOrder *current_head_index_ptr_1 =
        lock_free_queue_ref.consumer_uncommitted_peek(local_head_idx + 1);
    const QueueOrder *current_head_index_ptr_2 =
        lock_free_queue_ref.consumer_uncommitted_peek(local_head_idx + 2);
    const QueueOrder *current_head_index_ptr_3 =
        lock_free_queue_ref.consumer_uncommitted_peek(local_head_idx + 3);
    const QueueOrder *current_head_index_ptr_4 =
        lock_free_queue_ref.consumer_uncommitted_peek(local_head_idx + 4);
    const QueueOrder *current_head_index_ptr_5 =
        lock_free_queue_ref.consumer_uncommitted_peek(local_head_idx + 5);
    const QueueOrder *current_head_index_ptr_6 =
        lock_free_queue_ref.consumer_uncommitted_peek(local_head_idx + 6);
    const QueueOrder *current_head_index_ptr_7 =
        lock_free_queue_ref.consumer_uncommitted_peek(local_head_idx + 7);

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
      lock_free_queue_ref.publish_head_release(local_head_idx);
    }
  }
  lock_free_queue_ref.publish_head_release(local_head_idx);
  auto total_accumulator_sum =
      ((local_register_accumulator_0 + local_register_accumulator_1) +
       (local_register_accumulator_2 + local_register_accumulator_3) +
       (local_register_accumulator_4 + local_register_accumulator_5) +
       (local_register_accumulator_6 + local_register_accumulator_7));

  return;
}
