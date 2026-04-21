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
    mProducer.tail = 0;
    mConsumer.head = 0;
}

//static factory method for class is the buffer allocation
LockFreeSPSCQueue* LockFreeSPSCQueue::create(int capacity) {
    void* raw_memory_ptr = nullptr;
    size_t required_bytes = (capacity * sizeof(QueueOrder) + sizeof(Metadata_Producer) + sizeof(Metadata_Consumer));
    int returned_result = posix_memalign(&raw_memory_ptr, 64, required_bytes);
    assert(returned_result == 0 && "Posix memalign assertion failed \n");
    LockFreeSPSCQueue* obj = new (raw_memory_ptr) LockFreeSPSCQueue(capacity);
    obj->capacity = capacity;
    return obj;
}

void LockFreeSPSCQueue::destroy(LockFreeSPSCQueue* ptr) {
    ptr->~LockFreeSPSCQueue();
    free(ptr);
}

bool LockFreeSPSCQueue::push_order_into_queue(const QueueOrder& qOrder) {
    int next_prefetch_index = (mProducer.tail + L) & (capacity - 1);
    __builtin_prefetch(&q[next_prefetch_index], 1, 3);
    if (!(mProducer.tail + 1) & (capacity - 1) != mProducer.head_cached) {
        mProducer.head_cached = mConsumer.head.load(std::memory_order_acquire);
    }
    q[mProducer.tail & (capacity-1)] = qOrder;
    mProducer.tail.store(mProducer.tail+1, std::memory_order_release);
    assert(!queue_empty() && "Queue is empty.. \n");
    return true;
}

bool LockFreeSPSCQueue::pop_order_from_queue() {
    int next_prefetch_index = (mConsumer.head + L) & (capacity - 1);
    __builtin_prefetch(&q[next_prefetch_index], 0, 3);
    if (!(mConsumer.head != mConsumer.tail_cached)) {
        mConsumer.tail_cached = mProducer.tail.load(std::memory_order_acquire);
    }
    q[mConsumer.head & (capacity - 1)];
    mConsumer.head.store(mConsumer.head + 1, std::memory_order_release);
    return true;
}

int LockFreeSPSCQueue::get_front_order_id() {
    if (queue_empty()) {
        return 0;
    }
    int next_prefetch_index = (mConsumer.head + L) & (capacity - 1);
    __builtin_prefetch(&q[next_prefetch_index], 0, 3);
    return q[mConsumer.head & (capacity-1)].order_id;
}

void LockFreeSPSCQueue::display_all_orders_in_queue() {
    int next_prefetch_index = (mConsumer.head + L) & (capacity - 1);
    __builtin_prefetch(&q[next_prefetch_index], 0, 0);
    std::cout << "Current orders in the queue are: \n";
    for(int i = mConsumer.head; i < mProducer.tail; i++) {
        std::cout << "Order id: " << q[i & (capacity - 1)].order_id << " \n";
    }
    std::cout << "The current size of the queue is: " << get_queue_current_size() << " \n";
    return;
}

bool LockFreeSPSCQueue::queue_full() {
    if ((mProducer.tail - mConsumer.head) >= capacity) { return true; }
    return false;
}

bool LockFreeSPSCQueue::queue_empty() {
    if (mConsumer.head == mProducer.tail) { return true; }
    return false;
}

int LockFreeSPSCQueue::get_queue_current_size() {
    return mProducer.tail - mConsumer.head;
}

// LockFreeSPSCQueue::~LockFreeSPSCQueue() {
//     std::free(raw_buffer);
//     std::cout << "Allocated memory has been freed.. \n";
// }
