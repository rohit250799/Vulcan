#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <emmintrin.h>
#include <functional>
#include <pthread.h>
#include <thread>
#include <iostream>
#include <x86intrin.h>

#include "vulcan/lock_free_spsc_queue.hpp"
#include "vulcan/queue_core.hpp"

std::array<uint64_t, 100000000> raw_cycle_counts_array;
constexpr size_t TOTAL_OPS = 1000000000;

size_t build_telemetry_scaffold() {
    uint64_t start_cycles, end_cycles;
    uint64_t measurement_overhead = 1;
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
    alignas(64) std::array<QueueOrder, 8> queue_orders;
    populate_stack_buffer(queue_orders);
    uint64_t local_current_tail = 0;
    uint64_t local_current_head_cached = 0;
    uint64_t batch_count = 0;

    while (!m_start_ref.load(std::memory_order_acquire)) {
        _mm_pause();
    }

    warm_up_producer(lock_free_queue_ref, queue_orders);
    _mm_lfence();
    start_tsc = _rdtsc();
    for (size_t i = 0; i < TOTAL_OPS; i += 8) {
        uint64_t next_local_tail = (local_current_tail + 8) & 255;
        if (next_local_tail == local_current_head_cached) {
            while (next_local_tail == local_current_head_cached) {
                _mm_pause();
                local_current_head_cached = lock_free_queue_ref.load_head_acquire();
            }
        } // space is there now to push new QueueOrder
        lock_free_queue_ref.producer_uncommitted_push(queue_orders[0], local_current_tail);
        local_current_tail = next_local_tail;
        local_current_tail = (local_current_tail + 1) & 255;
        lock_free_queue_ref.producer_uncommitted_push(queue_orders[1], local_current_tail);
        local_current_tail = (local_current_tail + 1) & 255;
        lock_free_queue_ref.producer_uncommitted_push(queue_orders[2], local_current_tail);
        local_current_tail = (local_current_tail + 1) & 255;
        lock_free_queue_ref.producer_uncommitted_push(queue_orders[3], local_current_tail);
        local_current_tail = (local_current_tail + 1) & 255;
        lock_free_queue_ref.producer_uncommitted_push(queue_orders[4], local_current_tail);
        local_current_tail = (local_current_tail + 1) & 255;
        lock_free_queue_ref.producer_uncommitted_push(queue_orders[5], local_current_tail);
        local_current_tail = (local_current_tail + 1) & 255;
        lock_free_queue_ref.producer_uncommitted_push(queue_orders[6], local_current_tail);
        local_current_tail = (local_current_tail + 1) & 255;
        lock_free_queue_ref.producer_uncommitted_push(queue_orders[7], local_current_tail);
        local_current_tail = (local_current_tail + 1) & 255;

        lock_free_queue_ref.publish_tail_release(local_current_tail);
    }
    lock_free_queue_ref.publish_tail_release(local_current_tail);
    _mm_lfence();
    end_tsc = _rdtsc();
    auto producer_hot_path_burst_total_time_taken = end_tsc - start_tsc;
    auto timer_tax_adjusted_total_cycles = producer_hot_path_burst_total_time_taken - measurement_overhead;
    auto cycles_per_element = timer_tax_adjusted_total_cycles / TOTAL_OPS;
    std::cout << "The cycles per element in producer benchmark is: " << cycles_per_element << " \n";
    return;
}

void benchmark_consumer(LockFreeSPSCQueue<QueueOrder, 256>& lock_free_queue_ref, std::atomic<bool>& m_start_ref) { // this thread acts purely as the Matching Engine or Reflector
    pinThreadToCore(pthread_self(), 2);
    uint64_t start_tsc, end_tsc;
    uint64_t local_head_idx = 0; // tracks head location of queue for reading the incoming orders
    uint64_t local_tail_cached = 0;
    size_t batch_count = 0;
    double local_register_accumulator_0, local_register_accumulator_1, local_register_accumulator_2, local_register_accumulator_3, \
    local_register_accumulator_4, local_register_accumulator_5, local_register_accumulator_6, local_register_accumulator_7 = 0.0;

    while (!m_start_ref.load(std::memory_order_acquire)) {
        _mm_pause();
    }

    warm_up_consumer(lock_free_queue_ref);
    _mm_lfence();
    start_tsc = _rdtsc();
    for (size_t i = 0; i < TOTAL_OPS; i+=8) {
        if (local_head_idx == local_tail_cached) {
            while (local_head_idx == local_tail_cached) {
                _mm_pause();
                local_tail_cached = lock_free_queue_ref.load_tail_acquire();
            }
        }
        const QueueOrder* current_head_index_ptr_0 = lock_free_queue_ref.consumer_uncommitted_peek(local_head_idx);
        const QueueOrder* current_head_index_ptr_1 = lock_free_queue_ref.consumer_uncommitted_peek(local_head_idx + 1);
        const QueueOrder* current_head_index_ptr_2 = lock_free_queue_ref.consumer_uncommitted_peek(local_head_idx + 2);
        const QueueOrder* current_head_index_ptr_3 = lock_free_queue_ref.consumer_uncommitted_peek(local_head_idx + 3);
        const QueueOrder* current_head_index_ptr_4 = lock_free_queue_ref.consumer_uncommitted_peek(local_head_idx + 4);
        const QueueOrder* current_head_index_ptr_5 = lock_free_queue_ref.consumer_uncommitted_peek(local_head_idx + 5);
        const QueueOrder* current_head_index_ptr_6 = lock_free_queue_ref.consumer_uncommitted_peek(local_head_idx + 6);
        const QueueOrder* current_head_index_ptr_7 = lock_free_queue_ref.consumer_uncommitted_peek(local_head_idx + 7);


        local_register_accumulator_0 += current_head_index_ptr_0->price;
        local_register_accumulator_1 += current_head_index_ptr_1->price;
        local_register_accumulator_2 += current_head_index_ptr_2->price;
        local_register_accumulator_3 += current_head_index_ptr_3->price;
        local_register_accumulator_4 += current_head_index_ptr_4->price;
        local_register_accumulator_5 += current_head_index_ptr_5->price;
        local_register_accumulator_6 += current_head_index_ptr_6->price;
        local_register_accumulator_7 += current_head_index_ptr_7->price;

        local_head_idx = (local_head_idx + 8) & 255;
        batch_count ++;
        if ((batch_count & 7) == 0) {
            lock_free_queue_ref.publish_head_release(local_head_idx);
        }
    }
    lock_free_queue_ref.publish_head_release(local_head_idx);
    auto total_accumulator_sum = ((local_register_accumulator_0 + local_register_accumulator_1) + (local_register_accumulator_2 + local_register_accumulator_3) + \
        (local_register_accumulator_4 + local_register_accumulator_5) + (local_register_accumulator_6 + local_register_accumulator_7));
    _mm_lfence();
    end_tsc = _rdtsc();
    auto consumer_hot_path_burst_total_time_taken = end_tsc - start_tsc;
    auto timer_tax_adjusted_total_cycles = consumer_hot_path_burst_total_time_taken - measurement_overhead;
    auto cycles_per_element = timer_tax_adjusted_total_cycles / TOTAL_OPS;
    std::cout << "The cycles per element in consumer benchmark is: " << cycles_per_element << " \n";
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
