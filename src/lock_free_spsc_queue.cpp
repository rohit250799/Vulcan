#define _POSIX_C_SOURCE 200809L

#include <atomic>
#include <stdlib.h>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <sys/types.h>
#include "vulcan/lock_free_spsc_queue.hpp"

LockFreeSPSCQueue::LockFreeSPSCQueue(int capacity) 
    : capacity(capacity) {
    size_t required_bytes = capacity * sizeof(QueueOrder);
    int returned_result = posix_memalign(reinterpret_cast<void**>(&m_order), 64, required_bytes);   
    assert(m_order != nullptr && "Since memory allocation failed, can't create queue \n");
    assert(returned_result == 0 && "Memory allocation assertion failed, return non-0 number \n");
}

bool LockFreeSPSCQueue::push_order_into_queue(const QueueOrder& qOrder) {
    //uint local_tail = tail_cached;
    //uint local_head = head_cached;
    if (!((tail - head_cached) < capacity)) {
        //std::cout << "Queue appears to be full, reloading atomic head \n";
        head_cached = head.load(std::memory_order_acquire);
    }
    if (tail - head_cached >= capacity) { return false; }
    int next_prefetch_index = (tail + L) & (capacity - 1);
    __builtin_prefetch(&m_order[next_prefetch_index], 1, 3);
    m_order[tail & (capacity-1)] = qOrder;
    tail.store(tail+1, std::memory_order_release);
    assert(!queue_empty() && "Queue is empty.. \n");
    return true;
}

bool LockFreeSPSCQueue::pop_order_from_queue() {
    //uint local_tail = tail_cached;
    int next_prefetch_index = (head + L) & (capacity - 1);
    __builtin_prefetch(&m_order[next_prefetch_index], 0, 3);
    if (tail_cached == head) {
        //std::cout << "Queue appears to be empty, reloading the atomic tail \n";
        tail = tail.load(std::memory_order_acquire);
    }
    if (queue_empty()) { return false; }
    head.store(head + 1, std::memory_order_release);
    return true;
}

int LockFreeSPSCQueue::get_front_order_id() {
    if (queue_empty()) {
        //std::cout << "Since queue is empty, can't get front order id \n";
        return 0;
    }
    int next_prefetch_index = (head + L) & (capacity - 1);
    __builtin_prefetch(&m_order[next_prefetch_index], 0, 3);
    return m_order[head & (capacity-1)].order_id;
}

void LockFreeSPSCQueue::display_all_orders_in_queue() {
    int next_prefetch_index = (head + L) & (capacity - 1);
    __builtin_prefetch(&m_order[next_prefetch_index], 0, 0);
    std::cout << "Current orders in the queue are: \n";
    for(int i = head; i < tail; i++) {
        std::cout << "Order id: " << m_order[i & (capacity - 1)].order_id << " \n";
    }
    std::cout << "The current size of the queue is: " << get_queue_current_size() << " \n";
    return;
}

bool LockFreeSPSCQueue::queue_full() {
    if ((tail - head) >= capacity) { return true; }
    return false;
}

bool LockFreeSPSCQueue::queue_empty() {
    if (head == tail) { return true; }
    return false;
}

int LockFreeSPSCQueue::get_queue_current_size() {
    return tail - head;
}

LockFreeSPSCQueue::~LockFreeSPSCQueue() {
    std::free(raw_buffer);
    std::cout << "Allocated memory has been freed.. \n";
}