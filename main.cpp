#define _GNU_SOURCE
#include <array>

#include <chrono>
#include <emmintrin.h>

#include <atomic>
#include <cstdlib>
#include <functional>
#include <immintrin.h>

#include <cerrno>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <iomanip>
#include <pthread.h>
#include <sched.h>
#include <type_traits>
#include <thread>
#include <cpuid.h>

#include "vulcan/lock_free_spsc_queue.hpp"
#include "vulcan/queue_core.hpp"

void push_orders_to_queue(LockFreeSPSCQueue<QueueOrder, 256>& queue, std::atomic<bool>& m_start_ref, int core_id) {
    alignas(64) std::array<QueueOrder, 1024> queue_orders;
    populate_stack_buffer(queue_orders);
    pthread_t current_thread_id = pthread_self();
    pinThreadToCore(current_thread_id, core_id);
    assert(sched_getcpu() == core_id && "Assertion failed: Pinning is not working \n");
    
    while (!m_start_ref.load(std::memory_order_acquire)) {
        _mm_pause(); // notify LSU that the thread is in a spin-wait, reducing power consumption and preventing pipeline from being corrupted with speculative loads (evicting hot matching engine code from the L1i)
    }
    
    size_t local_current_tail = 0;
    size_t local_current_head_cached = 0;

    unsigned int eax, ebx, ecx, edx;
    __cpuid(0, eax, ebx, ecx, edx);
    
    const int TOTAL_BURST_ORDERS = 100000000;
    unsigned long long start_time = __rdtsc();
    
    for (size_t i = 0; i < TOTAL_BURST_ORDERS; ++i) {
        size_t next_loaal_tail = (local_current_tail + 1) & 255;
        if (next_loaal_tail == local_current_head_cached) {
            local_current_head_cached = queue.load_head_acquire();
        }
        size_t buffer_index = local_current_tail & 1023;
        queue.push_order_into_queue(queue_orders[buffer_index]);
        local_current_tail = (local_current_tail + 1) & 255;
    }    
    unsigned long long end_time = __rdtsc();
    std::cout << "Producer thread is warmed up for now.. \n";
    return;
}

void pop_orders_from_queue(LockFreeSPSCQueue<QueueOrder, 256>& queue, std::atomic<bool>& m_start_ref, int core_id) {
    pthread_t current_thread_id = pthread_self();
    pinThreadToCore(current_thread_id, core_id);
    assert(sched_getcpu() == core_id && "Assertion failed: Pinning is not working in consumer thread \n");
    
    size_t local_head_index = 0;
    size_t local_tail_cached_test = 0;

    while (!m_start_ref.load(std::memory_order_acquire)) {
        _mm_pause(); //same as in push_orders_to_queue function (waiting for thread to finish OS-level init, pinning to cores, already spinning and hot in their L1i)
    }
    const int TOTAL_BURST_ORDERS = 100000000;
    for (size_t i = 0; i < TOTAL_BURST_ORDERS; ++i) {
        auto* ptr = queue.peek_into_queue(local_head_index);
        if (ptr == nullptr) {
            local_tail_cached_test = queue.load_tail_acquire();
            _mm_pause();
        }
        queue.commit_pop_order_from_queue(ptr, local_head_index);
    }
    std::cout << "Consumer thread is warmed up right now.. \n";
    return;
}   

int main() {
    int core1 = 0;
    int core2 = 2;
    std::atomic<bool> m_start(false);
    
    LockFreeSPSCQueue<QueueOrder, 256>* my_queue = LockFreeSPSCQueue<QueueOrder, 256>::create();
    std::thread producer_thread(push_orders_to_queue, std::ref(*my_queue), std::ref(m_start), core1);
    std::thread consumer_thread(pop_orders_from_queue, std::ref(*my_queue), std::ref(m_start), core2);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    m_start.store(true, std::memory_order_release);
    
    producer_thread.join();
    consumer_thread.join();

    return 0;
}
