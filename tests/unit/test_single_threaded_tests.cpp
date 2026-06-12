// tests/unit/test_queue.cpp
// COMPILE: g++ -O3 -DNDEBUG test_queue.cpp -o test_queue.unit

#include <cassert>
#include <stdio.h>
#include "vulcan/lock_free_spsc_queue.hpp"
#include "../harness/test_macros.hpp"
#include "../harness/test_runner.hpp"
    
static int test_queue_empty_on_thread_creation() {
    LockFreeSPSCQueue<QueueOrder, 4> my_queue = LockFreeSPSCQueue<QueueOrder, 4>();
    TEST_ASSERT(my_queue.queue_empty(), "Assertion failed: Queue is not empty \n");
    return 0;
}

static int test_queue_full() {
    LockFreeSPSCQueue<QueueOrder, 4> my_queue = LockFreeSPSCQueue<QueueOrder, 4>();
    const QueueOrder& qOrder1 = QueueOrder{1, 122.43, 29};
    const QueueOrder& qOrder2 = QueueOrder{2, 252.3, 54};
    const QueueOrder& qOrder3 = QueueOrder{3, 12.7, 45};
    my_queue.push_order_into_queue(qOrder1);
    my_queue.push_order_into_queue(qOrder2);
    TEST_ASSERT(!my_queue.queue_full(), "Assertion failed: There is no space in the queue \n");
    my_queue.push_order_into_queue(qOrder3);
    TEST_ASSERT(my_queue.queue_full(), "Assertion Failed: Queue is still not full \n");
    return 0;
}

// Register tests manually (explicit control - no magic macros)
static TestCase tests[] = {
    {"queue_empty_on_creation", test_queue_empty_on_thread_creation},
    {"queue_full", test_queue_full},
    {nullptr, nullptr}  // Sentinel
};

int main(int argc, char** argv) {
    return run_tests(tests, argc, argv);
}
