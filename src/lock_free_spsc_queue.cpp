#include <cstdint>
#define _POSIX_C_SOURCE 200809L

#include <atomic>
#include <stdlib.h>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <sys/types.h>
#include "vulcan/lock_free_spsc_queue.hpp"

LockFreeSPSCQueue::LockFreeSPSCQueue(int capacity) : mProducer{head_cached=0}, mConsumer(.tail_cached=0), mConsumer(.capacity=capacity)
{
    assert(sizeof(Metadata_Producer) == 64 && "Size of metadata producer should be 64 bytes \n");
    assert(sizeof(Metadata_Consumer) == 64 && "Size of metadata consumer should be 64 bytes \n");
    assert(capacity == mConsumer.capacity && "Capacity should be the same as the argument \n");
    assert((mConsumer.capacity & (mConsumer.capacity-1)) == 0 && "Capacity should be a power of 2 \n");
    assert(reinterpret_cast<uintptr_t>(this)%64 == 0 && "Assertion failed: Non functional core \n");
    assert(mProducer.mask == mConsumer.mask && "Mask in both producer and consumer threads should be equal \n");
    assert(sizeof(QueueOrder) % 64 == 0 && "QueueOrders should be 64 bytes in size \n");
}

//static factory method for class is the buffer allocation
LockFreeSPSCQueue* LockFreeSPSCQueue::create(int capacity) {
    void* raw_memory_ptr = nullptr;
    size_t required_bytes = (capacity * sizeof(QueueOrder) + sizeof(Metadata_Producer) + sizeof(Metadata_Consumer));
    int returned_result = posix_memalign(&raw_memory_ptr, 64, required_bytes);
    assert(returned_result == 0 && "Posix memalign assertion failed \n");
    LockFreeSPSCQueue* obj = new (raw_memory_ptr) LockFreeSPSCQueue(capacity);
    obj->mConsumer.capacity = capacity;
    assert((obj->mConsumer.capacity & (obj->mConsumer.capacity-1)) == 0 && "Capacity should be a power of 2 \n");
    return obj;
}

void LockFreeSPSCQueue::destroy(LockFreeSPSCQueue* ptr) {
    ptr->~LockFreeSPSCQueue();
    free(ptr);
}

bool LockFreeSPSCQueue::push_order_into_queue(const QueueOrder& qOrder) {
    size_t current_tail = mProducer.tail.load(std::memory_order_relaxed);
    int next_tail = (current_tail+1) & (mProducer.mask);
    if (next_tail == mProducer.head_cached) {
        mProducer.head_cached = mConsumer.head.load(std::memory_order_acquire);
    }
    if (next_tail == mProducer.head_cached) { return false; }
    size_t zero_copy_memory_address = reinterpret_cast<uintptr_t>(reinterpret_cast<std::byte*>(this) + (64 * 2) + (current_tail * 64));
    new (reinterpret_cast<void*>(zero_copy_memory_address)) QueueOrder(qOrder);
    mProducer.tail.store(next_tail, std::memory_order_release);
    return true;
}

bool LockFreeSPSCQueue::pop_order_from_queue() {
    if (mConsumer.head != mConsumer.tail_cached) {
        mConsumer.head.store((mConsumer.head+1)&(mConsumer.mask), std::memory_order_release);
        return true;
    }
    mConsumer.tail_cached = mProducer.tail.load(std::memory_order_acquire);
    if (queue_empty()) { return false; }
    pop_order_from_queue();
}

int LockFreeSPSCQueue::get_front_order_id() {
    if (queue_empty()) {
        return 0;
    }
    int next_prefetch_index = (mConsumer.head + L) & (mConsumer.capacity - 1);
    __builtin_prefetch(&q[next_prefetch_index], 0, 3);
    return q[mConsumer.head & (mConsumer.capacity-1)].order_id;
}

void LockFreeSPSCQueue::display_all_orders_in_queue() {
    int next_prefetch_index = (mConsumer.head + L) & (mConsumer.capacity - 1);
    __builtin_prefetch(&q[next_prefetch_index], 0, 0);
    std::cout << "Current orders in the queue are: \n";
    for(int i = mConsumer.head; i < mProducer.tail; i++) {
        std::cout << "Order id: " << q[i & (mConsumer.capacity - 1)].order_id << " \n";
    }
    std::cout << "The current size of the queue is: " << get_queue_current_size() << " \n";
    return;
}

bool LockFreeSPSCQueue::queue_full() {
    if ((mProducer.tail+1)&(mConsumer.capacity-1) == mProducer.head_cached) { return true; }
    return false;
}

bool LockFreeSPSCQueue::queue_empty() {
    if (mConsumer.head == mConsumer.tail_cached) { return true; }
    return false;
}

int LockFreeSPSCQueue::get_queue_current_size() {
    return mProducer.tail - mConsumer.head;
}
