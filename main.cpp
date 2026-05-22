#define _GNU_SOURCE

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

#include "vulcan/lock_free_spsc_queue.hpp"

void pinThreadToCore(pthread_t thread_id, int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    
    int set_affinity_mask_result = pthread_setaffinity_np(thread_id, sizeof(cpu_set_t), &cpuset);
    if (set_affinity_mask_result != 0) {
        std::cerr << "Error calling pthread_setaffinity_np for core: " << core_id << "has error: " << EXIT_FAILURE << " \n";
    }
}

void push_orders_to_queue(LockFreeSPSCQueue<QueueOrder, 256>& queue, std::atomic<bool>& m_start_ref, int core_id) {
    pthread_t current_thread_id = pthread_self();
    pinThreadToCore(current_thread_id, core_id);
    assert(sched_getcpu() == core_id && "Assertion failed: Pinning is not working \n");
    
    while (!m_start_ref.load(std::memory_order_acquire)) {
        _mm_pause(); // notify LSU that the thread is in a spin-wait, reducing power consumption and preventing pipeline from being corrupted with speculative loads (evicting hot matching engine code from the L1i)
    }
    
    const int TOTAL_CYCLES = 1000000;
    QueueOrder dummy_instance = {100, 27.32, 102};
    for (size_t i = 0; i < 100000; ++i) {
        
    }
    std::cout << "Producer thread is warmed up for now.. \n";
    return;
}

void pop_orders_from_queue(LockFreeSPSCQueue<QueueOrder, 256>& queue, std::atomic<bool>& m_start_ref, int core_id) {
    pthread_t current_thread_id = pthread_self();
    pinThreadToCore(current_thread_id, core_id);
    assert(sched_getcpu() == core_id && "Assertion failed: Pinning is not working in consumer thread \n");

    while (!m_start_ref.load(std::memory_order_acquire)) {
        _mm_pause(); //same as in push_orders_to_queue function (waiting for thread to finish OS-level init, pinning to cores, already spinning and hot in their L1i)
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
