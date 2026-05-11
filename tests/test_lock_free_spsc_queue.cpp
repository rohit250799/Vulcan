#include <cassert>
#include <iostream>
#include <thread>
#include "vulcan/lock_free_spsc_queue.hpp"

auto* my_queue = LockFreeSPSCQueue<QueueOrder, 4>::create();

bool test_push_operation_in_queue(const QueueOrder& qOrder) {
    bool result = my_queue->push_order_into_queue(qOrder);
    return result;
}

bool test_queue_empty() {
    bool result = my_queue->queue_empty();
    return result;
}

bool test_queue_full() {
    bool result = my_queue->queue_full();
    return result;
}

void test_pop_operation_from_queue() {
    std::cout << "Entered the test pop operation from queue body.. \n";
    my_queue->pop_order_from_queue();
    my_queue->pop_order_from_queue();
    std::cout << "The current size of the queue is: " << my_queue->get_queue_current_size() << " \n";
    return;
}

void test_display_all_orders_in_queue() {
    return;
}

void consumer_thread_testing() {
    std::cout << "Entered the consumer thread body for testing \n";
    for (int i = 0;i<10;i++) {
        if (!my_queue->pop_order_from_queue()) {
            continue;
        }
        my_queue->get_front_order_id();
    }
    return;
}

bool check_queue_working_with_single_thread() {
    for (int i = 0; i < 1500; i++) {
        my_queue->push_order_into_queue({i, i*10+1, i*15+1});
    }
    return true;
}

void test_single_thread_boundary_validation() {
    auto* my_local_spsc_queue = LockFreeSPSCQueue<QueueOrder, 4>::create();
    assert(my_local_spsc_queue->push_order_into_queue({100, 22.32, 232}) == true && "Push assertion failed.. 100 can't be pushed \n");
    assert(my_local_spsc_queue->push_order_into_queue({200, 232.32, 21}) == true && "Push assertion failed.. 200 can't be pushed \n");
    assert(my_local_spsc_queue->push_order_into_queue({300, 2993.2, 12}) == true && "Push assertion failed.. 300 can't be pushed \n");
    //my_local_spsc_queue->display_all_orders_in_queue();
    assert(my_local_spsc_queue->queue_full() && "Assertion failed, queue is not full \n");
    assert(my_local_spsc_queue->push_order_into_queue({400, 212.32, 39}) == false && "Push assertion failed.. 400 can't be pushed as queue is full \n");
    //my_local_spsc_queue->display_all_orders_in_queue();
    assert(my_local_spsc_queue->pop_order_from_queue() == true && "Pop assertion failed, can't be popped \n");
    assert(my_local_spsc_queue->get_queue_current_size() == 2 && "Queue size assertion failed after 3 push and 1 pop \n");
    //my_local_spsc_queue->display_all_orders_in_queue();
    assert(my_local_spsc_queue->push_order_into_queue({500, 223.33, 10}) == true && "Push assertion failed.. 500 still can't be pushed \n");
    //my_local_spsc_queue->display_all_orders_in_queue();
    assert(my_local_spsc_queue->pop_order_from_queue() == true && "Pop assertion failed.. can't be popped \n");
    assert(my_local_spsc_queue->pop_order_from_queue() == true && "Pop assertion failed.. can't be popped \n");
    assert(my_local_spsc_queue->pop_order_from_queue() == true && "Pop assertion failed.. can't be popped \n");
    assert(my_local_spsc_queue->pop_order_from_queue() == false && "Pop assertion failed.. queue is not yet empty \n");
    //my_local_spsc_queue->display_all_orders_in_queue();
    assert(my_local_spsc_queue->pop_order_from_queue() == false && "Pop assertion failed as queue is empty \n");
    std::cout << "single thread boundary validation done \n";
    return;
}

void test_single_thread_wrap_around() {
    auto* my_local_spsc_queue = LockFreeSPSCQueue<QueueOrder, 4>::create();

    // Initial state
    assert(my_local_spsc_queue->queue_empty());

    // Fill queue (size 4, so 4th push should fail)
    assert(my_local_spsc_queue->push_order_into_queue({1, 22.32, 232}));
    assert(my_local_spsc_queue->push_order_into_queue({2, 232.32, 21}));
    assert(my_local_spsc_queue->push_order_into_queue({3, 2993.2, 12}));
    assert(!my_local_spsc_queue->push_order_into_queue({4, 212.32, 39})); // Queue full
    // Pop 2 items
    assert(my_local_spsc_queue->pop_order_from_queue());
    assert(my_local_spsc_queue->get_front_order_id() == 2);
    assert(my_local_spsc_queue->pop_order_from_queue());
    assert(my_local_spsc_queue->get_front_order_id() == 3);
    // Push 2 more (wrap around test)
    assert(my_local_spsc_queue->push_order_into_queue({4, 92.2, 23}));
    assert(my_local_spsc_queue->push_order_into_queue({5, 392.2, 23}));
    // Pop to verify wrap
    assert(my_local_spsc_queue->pop_order_from_queue());
    assert(my_local_spsc_queue->get_front_order_id() == 4);
    // Continue testing
    assert(my_local_spsc_queue->push_order_into_queue({6, 80.233, 23}));
    assert(my_local_spsc_queue->pop_order_from_queue());
    assert(my_local_spsc_queue->get_front_order_id() == 5);
    assert(my_local_spsc_queue->pop_order_from_queue());
    assert(my_local_spsc_queue->get_front_order_id() == 6);
    assert(my_local_spsc_queue->pop_order_from_queue());
    // Queue should be empty now
    assert(my_local_spsc_queue->queue_empty());
    // Pop from empty queue should fail
    assert(!my_local_spsc_queue->pop_order_from_queue());
}       

void test_single_thread_full_or_empty_edge() {
    auto* my_local_spsc_queue = LockFreeSPSCQueue<QueueOrder, 2>::create();
    
    // Empty state
    assert(my_local_spsc_queue->queue_empty() == true);
    assert(my_local_spsc_queue->queue_full() == false);
    
    // Push 1 element (queue becomes full)
    assert(my_local_spsc_queue->push_order_into_queue({1, 193.33, 324}) == true);
    assert(my_local_spsc_queue->queue_empty() == false);
    assert(my_local_spsc_queue->queue_full() == true);  // Full after 1 element
    
    // Try to push 2nd element (should fail - queue full)
    assert(my_local_spsc_queue->push_order_into_queue({2, 19.32, 484}) == false);
    
    // Pop the element (queue becomes empty again)
    assert(my_local_spsc_queue->pop_order_from_queue() == true);
    assert(my_local_spsc_queue->queue_empty() == true);
    assert(my_local_spsc_queue->queue_full() == false);
    
    // Now push should work again
    assert(my_local_spsc_queue->push_order_into_queue({2, 19.32, 484}) == true);
    assert(my_local_spsc_queue->queue_full() == true);
}

void test_concurrent_correctness_with_verification() {
    std::thread consumer_thread(consumer_thread_testing);
    return;
}
