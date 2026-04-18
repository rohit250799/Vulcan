#include <iostream>
#include <thread>
#include "vulcan/lock_free_spsc_queue.hpp"

void test_move_into_vector();

void test_move_assignment();
void test_order_allocation();
bool test_queue_empty();
bool test_queue_full();
bool test_push_operation_in_queue(const QueueOrder& qOrder);
bool test_pop_operation_from_queue();
void test_display_all_orders_in_queue();
void test_single_thread_boundary_validation();
void test_single_thread_wrap_around();
void test_single_thread_full_or_empty_edge();

void producer_thread_testing() {
    assert(!test_queue_full() && "Queue is full \n");
    test_push_operation_in_queue({172, 234.343, 20});
    test_push_operation_in_queue({52, 24.3003, 320});
    test_push_operation_in_queue({45, 24.3003, 320});
    return;
}

int main() {
    std::cout << "[TEST] Running tests...\n";
    // test_move_into_vector();
    // test_move_assignment();
    // test_order_allocation();
    // test_queue_empty();
    // test_pop_operation_from_queue();
    // test_queue_empty();
    std::cout << "Testing SPSC queue... \n";
    // std::thread prod_thread(producer_thread_testing);
    // std::thread consumer_thread(consumer_thread_testing);
    // prod_thread.detach();
    // consumer_thread.detach();
    test_single_thread_boundary_validation();
    //bool queue_emptyness_state = test_queue_empty();
    //std::cout << "The queue emptyness state is: " << queue_emptyness_state << " \n";
    // prod_thread.join();
    // consumer_thread.join();
    test_single_thread_wrap_around();
    test_single_thread_full_or_empty_edge();
    std::cout << "[TEST] All tests passed... \n";
    test_display_all_orders_in_queue();
    return 0;
}