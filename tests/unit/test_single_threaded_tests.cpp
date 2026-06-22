// tests/unit/test_queue.cpp
// COMPILE: g++ -O3 -DNDEBUG test_queue.cpp -o test_queue.unit

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <emmintrin.h>
#include <functional>
#include <thread>
#include "vulcan/lock_free_spsc_queue.hpp"
#include "../harness/test_macros.hpp"
#include "../harness/test_runner.hpp"

static int test_queue_empty_on_thread_creation() {
    LockFreeSPSCQueue<QueueOrder, 4> my_queue = LockFreeSPSCQueue<QueueOrder, 4>();
    TEST_ASSERT(my_queue.queue_empty(), "Assertion failed: Queue is not empty \n");
    return 0;
}

std::atomic<bool> pop_empty_queue_result{false};
std::atomic<bool> push_to_full_queue_result{false};

void test_emulate_consumer_thread_in_pop_empty_operation(LockFreeSPSCQueue<QueueOrder, 4>& my_queue_ref) {
    uint64_t local_head_index = 0;
    uint64_t local_tail_cached = 0;
    const QueueOrder* current_head_index_pointer = my_queue_ref.consumer_uncommitted_peek(0);
    assert(current_head_index_pointer->order_id == 1 && "Assertion failed: Order id of the head order is not what was provided \n");
    local_head_index = (local_head_index + 1) & 3;
    my_queue_ref.publish_head_release(local_head_index);
    assert(my_queue_ref.queue_empty() && "Assertion failed: Queue is still not empty \n");
    pop_empty_queue_result.store(true, std::memory_order_release);
    return;
}

void test_emulate_push_operation_after_full_capacity(LockFreeSPSCQueue<QueueOrder, 4>& my_queue_ref) {
    // starts working after main thread fills queue fully   (we should not be able to return from this function till we push an element)
    uint64_t local_current_tail = my_queue_ref.load_tail_acquire(); // 3 
    QueueOrder qOrder4 = QueueOrder{4, 2.723, 103};
    uint64_t next_local_tail = (local_current_tail + 1) & 3; //0
    uint64_t local_current_head_cached = my_queue_ref.load_head_acquire();
    assert(next_local_tail == local_current_head_cached && "Assertion failed: Next local tail is not the same as local current head cached \n");
    if (next_local_tail == local_current_head_cached) {
        while (local_current_tail == local_current_head_cached) {
            _mm_pause();
            local_current_head_cached = my_queue_ref.load_head_acquire();
        }
    }
    my_queue_ref.producer_uncommitted_push(qOrder4, local_current_tail);
    local_current_tail = (local_current_tail + 1) & 3;
    my_queue_ref.publish_tail_release(local_current_tail);
    push_to_full_queue_result.store(true, std::memory_order_release);
    return;
}

static int test_pop_from_empty_queue_fails() {
    //using a dual thread because in empty queue, single thread would be stuck in a spin-wait forever
    // test condition - pop blocks on empty and succeeds when there is element
    LockFreeSPSCQueue<QueueOrder, 4>* my_queue = LockFreeSPSCQueue<QueueOrder, 4>::create();
    uint64_t local_current_tail = 0;
    TEST_ASSERT(my_queue->queue_empty(), "Assertion failed: Queue is not empty on creation \n");
    QueueOrder qOrder1 = QueueOrder{1, 122.43, 29};
    std::thread consumer_emulator_thread(test_emulate_consumer_thread_in_pop_empty_operation, std::ref(*my_queue));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    my_queue->producer_uncommitted_push(qOrder1, 0);
    my_queue->publish_tail_release(local_current_tail);
    TEST_ASSERT_EQ(my_queue->queue_empty(), false, "Assertion failed: Queue is still empty \n");
    TEST_ASSERT_EQ(consumer_emulator_thread.joinable(), true, "Assertion failed: Consumer thread not joinable for pop \n");
    consumer_emulator_thread.join();
    TEST_ASSERT_EQ(pop_empty_queue_result.load(std::memory_order_acquire), true, "Assertion failed: Pop operation failed inside consumer thread body \n");
    return 0;
}

static int test_queue_full() {
    LockFreeSPSCQueue<QueueOrder, 4>* my_queue = LockFreeSPSCQueue<QueueOrder, 4>::create();
    uint64_t local_current_tail = 0;
    QueueOrder qOrder1 = QueueOrder{1, 122.43, 29};
    QueueOrder qOrder2 = QueueOrder{2, 252.3, 54};
    QueueOrder qOrder3 = QueueOrder{3, 12.7, 45};
    QueueOrder qOrder4 = QueueOrder{4, 2.723, 103};
    my_queue->producer_uncommitted_push(qOrder1, local_current_tail);
    local_current_tail = (local_current_tail + 1) & 3;
    my_queue->producer_uncommitted_push(qOrder2, local_current_tail);
    local_current_tail = (local_current_tail + 1) & 3;
    my_queue->publish_tail_release(local_current_tail);
    TEST_ASSERT(!my_queue->queue_full(), "Assertion failed: There is no space in the queue \n");
    my_queue->producer_uncommitted_push(qOrder3, local_current_tail);
    local_current_tail = (local_current_tail + 1) & 3;
    my_queue->publish_tail_release(local_current_tail);
    TEST_ASSERT_EQ(my_queue->queue_full(), true, "Assertion failed: Queue is still not full \n");
    return 0;
}

void pop_element_from_queue(LockFreeSPSCQueue<QueueOrder, 4>& my_queue_ref) {
    uint64_t local_head_index = my_queue_ref.load_head_acquire();
    const QueueOrder* current_head_index_ptr = my_queue_ref.consumer_uncommitted_peek(local_head_index);
    my_queue_ref.publish_head_release(local_head_index);
    return;
}

static int test_fill_to_capacity() {
    //when we try to push the Nth element to the queue, it should be blocked (stuck in spin-wait). Test for few seconds, if still blocked, test successful
    LockFreeSPSCQueue<QueueOrder, 4>* my_queue = LockFreeSPSCQueue<QueueOrder, 4>::create();
    uint64_t local_current_tail = 0;
    QueueOrder qOrder1 = QueueOrder{1, 122.43, 29};
    QueueOrder qOrder2 = QueueOrder{2, 252.3, 54};
    QueueOrder qOrder3 = QueueOrder{3, 12.7, 45};
    QueueOrder qOrder5 = QueueOrder{5, 22.3, 122};
    my_queue->producer_uncommitted_push(qOrder1, local_current_tail);
    local_current_tail = (local_current_tail + 1) & 3; // current = 1
    my_queue->producer_uncommitted_push(qOrder2, local_current_tail);
    local_current_tail = (local_current_tail + 1) & 3; // current = 2
    my_queue->producer_uncommitted_push(qOrder3, local_current_tail);
    local_current_tail = (local_current_tail + 1) & 3; // current = 3
    my_queue->publish_tail_release(local_current_tail);
    TEST_ASSERT_EQ(my_queue->queue_full(), true, "Assertion failed: Queue still not full \n");
    // at this step, the assertion that the Queue is full should pass. Now, when we try to push another element, queue should be stuck in spin-wait
    std::thread producer_emulator_thread(test_emulate_push_operation_after_full_capacity, std::ref(*my_queue));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    TEST_ASSERT_EQ(push_to_full_queue_result.load(std::memory_order_acquire), true, "Assertion failed: Producer emulator thread successfully pushed to full queue \n");
    pop_element_from_queue(*my_queue);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    TEST_ASSERT_EQ(producer_emulator_thread.joinable(), true, "Assertion failed: Producer emulator thread is still not joinable \n");
    producer_emulator_thread.join();
    return 0;
}

// Register tests manually (explicit control - no magic macros)
static TestCase tests[] = {
    {"queue_empty_on_creation", test_queue_empty_on_thread_creation},
    {"pop_from_empty_queue", test_pop_from_empty_queue_fails},
    {"queue_full", test_queue_full},
    {"fill_to_capacity", test_fill_to_capacity},
    {nullptr, nullptr}  // Sentinel
};
 
int main() {
    int passed = 0, failed = 0;
    for (int i = 0; tests[i].name != nullptr; i++) {
        printf("[  RUN  ] %s\n", tests[i].name);
        if (tests[i].func() == 0) {
            printf("[  OK  ]\n");
            passed++;
        } else {
            printf("[  FAILED  ]\n");
            failed++;
        }
    }
    printf("\nPassed: %d, Failed: %d\n", passed, failed);
    return failed ? 1 : 0;
}


