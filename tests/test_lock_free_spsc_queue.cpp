#include <cassert>
#include <iostream>
#include <thread>
#include "vulcan/lock_free_spsc_queue.hpp"

LockFreeSPSCQueue spsc_queue(1024);

bool test_push_operation_in_queue(const QueueOrder& qOrder) {
    bool result = spsc_queue.push_order_into_queue(qOrder);
    return result;
}

bool test_queue_empty() {
    bool result = spsc_queue.queue_empty();
    return result;
}

bool test_queue_full() {
    bool result = spsc_queue.queue_full();
    return result;
}

void test_pop_operation_from_queue() {
    std::cout << "Entered the test pop operation from queue body.. \n";
    spsc_queue.pop_order_from_queue();
    spsc_queue.pop_order_from_queue();
    std::cout << "The current size of the queue is: " << spsc_queue.get_queue_current_size() << " \n";
    return;
}

void test_display_all_orders_in_queue() {
    spsc_queue.display_all_orders_in_queue();
    return;
}

void consumer_thread_testing() {
    std::cout << "Entered the consumer thread body for testing \n";
    for (int i = 0;i<10;i++) {
        if (!spsc_queue.pop_order_from_queue()) {
            continue;
        }
        spsc_queue.get_front_order_id();
    }
    return;
}

bool check_queue_working_with_single_thread() {
    for (int i = 0; i < 1500; i++) {
        spsc_queue.push_order_into_queue({i, i*10+1, i*15+1});
    }
    return true;
}

void test_single_thread_boundary_validation() {
    LockFreeSPSCQueue local_spsc_queue(4);
    assert(local_spsc_queue.push_order_into_queue({100, 22.32, 232}) == true && "Push assertion failed.. 100 can't be pushed \n");
    assert(local_spsc_queue.push_order_into_queue({200, 232.32, 21}) == true && "Push assertion failed.. 200 can't be pushed \n");
    assert(local_spsc_queue.push_order_into_queue({300, 2993.2, 12}) == true && "Push assertion failed.. 300 can't be pushed \n");
    assert(local_spsc_queue.push_order_into_queue({400, 212.32, 39}) == true && "Push assertion failed.. 400 can't be pushed \n");
    assert(local_spsc_queue.queue_full() && "Queue is not full \n");
    assert(local_spsc_queue.push_order_into_queue({500, 223.33, 10}) == false && "Push assertion failed.. 500 can't be pushed as queue is full \n");
    assert(local_spsc_queue.pop_order_from_queue() == true && "Pop assertion failed, can't be popped \n");
    assert(local_spsc_queue.get_queue_current_size() == 3 && "Queue size assertion failed after 3 push and 1 pop \n");
    local_spsc_queue.display_all_orders_in_queue();
    assert(local_spsc_queue.push_order_into_queue({500, 223.33, 10}) == true && "Push assertion failed.. 500 still can't be pushed \n");
    local_spsc_queue.display_all_orders_in_queue();
    assert(local_spsc_queue.pop_order_from_queue() == true && "Pop assertion failed.. can't be popped \n");
    assert(local_spsc_queue.pop_order_from_queue() == true && "Pop assertion failed.. can't be popped \n");
    assert(local_spsc_queue.pop_order_from_queue() == true && "Pop assertion failed.. can't be popped \n");
    assert(local_spsc_queue.pop_order_from_queue() == true && "Pop assertion failed.. can't be popped \n");
    local_spsc_queue.display_all_orders_in_queue();
    assert(local_spsc_queue.pop_order_from_queue() == false && "Pop assertion failed as queue is empty \n");
    std::cout << "single thread boundary validation done \n";
    return;
}

void test_single_thread_wrap_around() {
    LockFreeSPSCQueue local_spsc_queue(4);
    assert(local_spsc_queue.push_order_into_queue({1, 22.32, 232}) == true && "Push assertion failed.. 1 can't be pushed \n");
    assert(local_spsc_queue.push_order_into_queue({2, 232.32, 21}) == true && "Push assertion failed.. 2 can't be pushed \n");
    assert(local_spsc_queue.push_order_into_queue({3, 2993.2, 12}) == true && "Push assertion failed.. 3 can't be pushed \n");
    assert(local_spsc_queue.push_order_into_queue({4, 212.32, 39}) == true && "Push assertion failed.. 4 can't be pushed \n");
    local_spsc_queue.display_all_orders_in_queue();
    assert(local_spsc_queue.pop_order_from_queue() == true && "Pop assertion failed \n");
    assert(local_spsc_queue.get_front_order_id() == 2);
    assert(local_spsc_queue.pop_order_from_queue() == true && "Pop assertion failed \n");
    assert(local_spsc_queue.get_front_order_id() == 3);
    local_spsc_queue.display_all_orders_in_queue();
    assert(local_spsc_queue.push_order_into_queue({5, 92.2, 23}) == true && "Push assertion failed.. 5 can't be pushed \n");
    assert(local_spsc_queue.push_order_into_queue({6, 392.2, 23}) == true && "Push assertion failed.. 6 can't be pushed \n");
    local_spsc_queue.display_all_orders_in_queue();
    assert(local_spsc_queue.pop_order_from_queue() == true && "Pop assertion failed \n");
    assert(local_spsc_queue.get_front_order_id() == 4);
    assert(local_spsc_queue.push_order_into_queue({7, 80.233, 23}) == true && "Push assertion failed.. 7 can't be pushed \n");
    local_spsc_queue.display_all_orders_in_queue();
    assert(local_spsc_queue.pop_order_from_queue() == true && "Pop assertion failed \n");
    assert(local_spsc_queue.get_front_order_id() == 5);
    assert(local_spsc_queue.pop_order_from_queue() == true && "Pop assertion failed.. \n");
    assert(local_spsc_queue.get_front_order_id() == 6);
    assert(local_spsc_queue.pop_order_from_queue() == true && "Pop assertion failed.. \n");
    assert(local_spsc_queue.get_front_order_id() == 7);
    assert(local_spsc_queue.pop_order_from_queue() == true);
    assert(local_spsc_queue.queue_empty());
    return;
}

void test_single_thread_full_or_empty_edge() {
    LockFreeSPSCQueue local_spsc_queue(1);
    assert(local_spsc_queue.queue_empty() == true && "Queue emptyness assertion failed \n");
    assert(local_spsc_queue.queue_full() == false && "Queue fullness assertion failed \n");
    assert(local_spsc_queue.push_order_into_queue({42, 193.33, 324}) == true && "Push assertion failed, 42 can't be pushed \n");
    assert(local_spsc_queue.queue_empty() == false && "Queue emptyness assertion failed \n");
    assert(local_spsc_queue.queue_full() == true && "Queue fullness assertion failed \n");
    assert(local_spsc_queue.push_order_into_queue({99, 19.32, 484}) == false && "Push assertion failed, 99 can't be pushed \n");
    assert(local_spsc_queue.pop_order_from_queue() == true && "Queue pop assertion failed \n");
    assert(local_spsc_queue.queue_empty() == true && "Queue emptyness assertion failed \n");
    assert(local_spsc_queue.queue_full() == false && "Queue fullness assertion failed \n");
    return;
}

void test_concurrent_correctness_with_verification() {
    std::thread consumer_thread(consumer_thread_testing);
    return;
}