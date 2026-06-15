// tests/unit/test_queue.cpp
// COMPILE: g++ -O3 -DNDEBUG test_queue.cpp -o test_queue.unit

#include <cassert>
#include <cstddef>
#include <cstdint>
#include "vulcan/lock_free_spsc_queue.hpp"
#include "../harness/test_macros.hpp"
#include "../harness/test_runner.hpp"

static int test_queue_empty_on_thread_creation() {
    LockFreeSPSCQueue<QueueOrder, 4> my_queue = LockFreeSPSCQueue<QueueOrder, 4>();
    TEST_ASSERT(my_queue.queue_empty(), "Assertion failed: Queue is not empty \n");
    return 0;
}

static int test_pop_from_empty_queue_fails() {
    LockFreeSPSCQueue<QueueOrder, 4>* my_queue = LockFreeSPSCQueue<QueueOrder, 4>::create();
    TEST_ASSERT(my_queue->queue_empty(), "Assertion failed: Queue is not empty on creation \n");
    
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

// Register tests manually (explicit control - no magic macros)
static TestCase tests[] = {
    {"queue_empty_on_creation", test_queue_empty_on_thread_creation},
    {"queue_full", test_queue_full},
    {nullptr, nullptr}  // Sentinel
};
 
int main() {
    int passed = 0, failed = 0;
    for (int i = 0; tests[i].name != nullptr; i++) {
        printf("[ RUN    ] %s\n", tests[i].name);
        if (tests[i].func() == 0) {
            printf("[       OK ]\n");
            passed++;
        } else {
            printf("[  FAILED  ]\n");
            failed++;
        }
    }
    printf("\nPassed: %d, Failed: %d\n", passed, failed);
    return failed ? 1 : 0;
}


