#include <array>
#include <cstddef>
#include <pthread.h>
#include <cerrno>
#include <iostream>
#include <cassert>

void pinThreadToCore(pthread_t thread_id, int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    int set_affinity_mask_result = pthread_setaffinity_np(thread_id, sizeof(cpu_set_t), &cpuset);
    if (set_affinity_mask_result != 0) {
        std::cerr << "Error calling pthread_setaffinity_np for core: " << core_id << "has error: " << EXIT_FAILURE << " \n";
    }
    // Ensuring the thread is allowed to run on the target core
    bool is_set = CPU_ISSET(core_id, &cpuset);
    // Ensuring thread is ONLY pinned to this core (affinity count == 1)
    int total_cores_in_mask = CPU_COUNT(&cpuset);
    assert(is_set && total_cores_in_mask == 1 && "Thread is not strictly pinned to the target core!");
}

bool populate_stack_buffer(std::array<QueueOrder, 1024>& my_queueOrder) {
    for (size_t i = 0; i < 1024; ++i) {
        my_queueOrder[i] = {i, (i+1)*1.33, i+100};
    }
    assert(my_queueOrder[1023].order_id == 1023 && "Assertion failed: Stack buffer not populated correctly \n");
    return true;
}

void warm_up_producer(LockFreeSPSCQueue<QueueOrder, 256>& lock_free_queue_ref, std::array<QueueOrder, 1024>& queue_orders_ref) {
    size_t local_current_tail = 0;
    size_t local_current_head_cached = 0;
    populate_stack_buffer(queue_orders_ref);

    for (size_t i = 0; i < 1000000; ++i) {
        size_t next_local_tail = (local_current_tail + 1) & 255;
        if (next_local_tail == local_current_head_cached) {
            local_current_head_cached = lock_free_queue_ref.load_head_acquire();
        }
        size_t buffer_index = local_current_tail & 1023;
        while (!lock_free_queue_ref.push_order_into_queue(queue_orders_ref[buffer_index])) {
            _mm_pause();
        }
        local_current_tail = (local_current_tail + 1) & 255;
    }
}

void warm_up_consumer(LockFreeSPSCQueue<QueueOrder, 256>& lock_free_queue_ref) {
    size_t head_idx = 0;
    volatile double local_register_accumulator = 0;
    for (size_t i = 0; i < 1000000; ++i) {
        const QueueOrder* current_head_index_ptr = lock_free_queue_ref.peek_into_queue(head_idx);
        while (!current_head_index_ptr) {
            _mm_pause();
            current_head_index_ptr = lock_free_queue_ref.peek_into_queue(head_idx);
        }
        local_register_accumulator += current_head_index_ptr->price;
        lock_free_queue_ref.commit_pop_order_from_queue(current_head_index_ptr, head_idx);
        head_idx = (head_idx + 1) & 255;
    }
}
