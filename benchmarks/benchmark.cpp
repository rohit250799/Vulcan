#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <emmintrin.h>
#include <functional>
#include <pthread.h>
#include <thread>
#include <x86intrin.h>

#include "vulcan/lock_free_spsc_queue.hpp"
#include "vulcan/queue_core.hpp"

std::array<uint64_t, 100000000> raw_cycle_counts_array;
constexpr size_t TOTAL_OPS = 100000000;

size_t build_telemetry_scaffold() {
    uint64_t start_cycles, end_cycles, measurement_overhead;
    
    for (size_t i = 0; i < 100000000; ++i) {
        raw_cycle_counts_array[i] = 0;
    }
    
    for (size_t i = 0; i < 100000000; ++i) {
        _mm_lfence();
        start_cycles = __rdtsc();
        
        _mm_lfence();
        end_cycles = __rdtsc();
        
        size_t current_measurement = end_cycles - start_cycles;
        size_t condition = (current_measurement < measurement_overhead);
        measurement_overhead = (condition * current_measurement) + (!condition * measurement_overhead);
    }
    return measurement_overhead;
}

const size_t measurement_overhead = build_telemetry_scaffold();

void benchmark_producer(LockFreeSPSCQueue<QueueOrder, 256>& lock_free_queue_ref, std::atomic<bool>& m_start_ref) { // this thread acts as a Order Generator and the Timekeeper
    uint64_t start_tsc, end_tsc;
    pinThreadToCore(pthread_self(), 0);
    alignas(64) std::array<QueueOrder, 1024> queue_orders;
    populate_stack_buffer(queue_orders);
    size_t local_current_tail = 0;
    size_t local_current_head_cached = 0;
    
    while (!m_start_ref.load(std::memory_order_acquire)) {
        _mm_pause();
    }

    _mm_lfence();
    start_tsc = _rdtsc();
    for (size_t i = 0; i < TOTAL_OPS; ++i) {
        size_t next_local_tail = (local_current_tail + 1) & 255;
        if (next_local_tail == local_current_head_cached) {
            local_current_head_cached = lock_free_queue_ref.load_head_acquire();
        }
        size_t buffer_index = local_current_tail & 1023;
        while (!lock_free_queue_ref.push_order_into_queue(queue_orders[buffer_index])) {
            _mm_pause();
        }
        local_current_tail = (local_current_tail + 1) & 255;
        raw_cycle_counts_array[i] = (end_tsc - start_tsc) - measurement_overhead;
    }
    end_tsc = _rdtsc();
    _mm_lfence();
    auto producer_hot_path_burst_total_time_taken = end_tsc - start_tsc;
    return;
}

void benchmark_consumer(LockFreeSPSCQueue<QueueOrder, 256>& lock_free_queue_ref, std::atomic<bool>& m_start_ref) { // this thread acts purely as the Matching Engine or Reflector 
    pinThreadToCore(pthread_self(), 2);
    uint64_t start_tsc, end_tsc;
    size_t head_idx = 0; // tracks head location of Queue_FWD for reading the incoming orders
    volatile double local_register_accumulator = 0.0;
    
    while (!m_start_ref.load(std::memory_order_acquire)) {
        _mm_pause();
    }
    
    _mm_lfence();
    start_tsc = _rdtsc();
    for (size_t i = 0; i < TOTAL_OPS; ++i) {
        const QueueOrder* current_head_index_ptr = lock_free_queue_ref.peek_into_queue(head_idx);
        while (!current_head_index_ptr) {
            _mm_pause();
            current_head_index_ptr = lock_free_queue_ref.peek_into_queue(head_idx);
        }
        local_register_accumulator += current_head_index_ptr->price;
        lock_free_queue_ref.commit_pop_order_from_queue(current_head_index_ptr, head_idx);
        head_idx = (head_idx + 1) & 255;
    }
    _mm_lfence();
    end_tsc = _rdtsc();
    return;
}

int main() {
    auto* my_Lock_Free_Queue = LockFreeSPSCQueue<QueueOrder, 256>::create();
    alignas(64) std::atomic<bool> m_start(false);
    
    std::thread producer_benchmark_thread(benchmark_producer, std::ref(*my_Lock_Free_Queue), std::ref(m_start));
    std::thread consumer_benchmark_thread(benchmark_consumer, std::ref(*my_Lock_Free_Queue), std::ref(m_start));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    m_start.store(true, std::memory_order_release);
    
    producer_benchmark_thread.join();
    consumer_benchmark_thread.join();
}

