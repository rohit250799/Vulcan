#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <limits>
#include <new>
#include <stdatomic.h>

struct alignas(64) QueueOrder {
    uint64_t order_id;
    double price;
    uint32_t quantity;
};

template <typename T, size_t capacity>
class LockFreeSPSCQueue {
    static_assert((capacity & (capacity - 1)) == 0, "Capacity must be a power of two");
    public:
    struct alignas(64) Metadata_Producer {
        std::atomic<size_t> tail = 0;
        size_t head_cached;
        static constexpr size_t mask = capacity - 1;
    };

    struct alignas(64) Metadata_Consumer {
        std::atomic<size_t> head = 0;
        size_t tail_cached;
        static constexpr size_t mask = capacity - 1;
    };

    LockFreeSPSCQueue();
    ~LockFreeSPSCQueue() = default;
    LockFreeSPSCQueue(const LockFreeSPSCQueue&) = delete;
    LockFreeSPSCQueue& operator=(const LockFreeSPSCQueue&) = delete;
    bool push_order_into_queue(const QueueOrder& qOrder);
    static LockFreeSPSCQueue* create();
    static void destroy(LockFreeSPSCQueue* ptr);
    bool pop_order_from_queue();
    bool queue_full();
    bool queue_empty();
    int get_queue_current_size();
    int get_front_order_id();
    T* buffer();

    private:
    Metadata_Producer mProducer;
    Metadata_Consumer mConsumer;
};

template<typename T, size_t capacity>
T* LockFreeSPSCQueue<T, capacity>::buffer() {
    return reinterpret_cast<T*>(reinterpret_cast<char*>(this) + 2 * 64);
}

template<typename T, size_t capacity>
LockFreeSPSCQueue<T, capacity>::LockFreeSPSCQueue() {
    assert(sizeof(Metadata_Producer) == 64 && "Size of metadata producer should be 64 bytes \n");
    assert(sizeof(Metadata_Consumer) == 64 && "Size of metadata consumer should be 64 bytes \n");
    assert(reinterpret_cast<uintptr_t>(this)%64 == 0 && "Assertion failed: Non functional core \n");
    assert(mProducer.mask == mConsumer.mask && "Mask in both producer and consumer threads should be equal \n");
    assert(sizeof(QueueOrder) % 64 == 0 && "QueueOrders should be 64 bytes in size \n");
}

//static factory method for class is the buffer allocation
template<typename T, size_t capacity>
LockFreeSPSCQueue<T, capacity>* LockFreeSPSCQueue<T, capacity>::create() {
    void* raw_memory_ptr = nullptr;
    size_t required_bytes = (capacity * sizeof(QueueOrder) + sizeof(Metadata_Producer) + sizeof(Metadata_Consumer));
    int returned_result = posix_memalign(&raw_memory_ptr, 64, required_bytes);
    assert(returned_result == 0 && "Posix memalign assertion failed \n");
    LockFreeSPSCQueue* obj = new (raw_memory_ptr) LockFreeSPSCQueue();
    return obj;
}

template<typename T, size_t capacity>
void LockFreeSPSCQueue<T, capacity>::destroy(LockFreeSPSCQueue* ptr) {
    ptr->~LockFreeSPSCQueue();
    free(ptr);
}

template<typename T, size_t capacity>
bool LockFreeSPSCQueue<T, capacity>::push_order_into_queue(const QueueOrder& qOrder) {
    size_t tail = mProducer.tail.load(std::memory_order_relaxed);
    size_t next_tail = (tail+1) & (capacity - 1);
    size_t head = mProducer.head_cached;
    if (next_tail == mProducer.head_cached) {
        //queue appears to be full
        head = mConsumer.head.load(std::memory_order_acquire);
        mProducer.head_cached = head;
        if (next_tail == mProducer.head_cached) { return false; }
    }
    auto buffer_address = reinterpret_cast<std::byte*>(this) + 128 + (tail * 64);
    new (reinterpret_cast<void*>(buffer_address)) QueueOrder(qOrder);
    mProducer.tail.store(next_tail, std::memory_order_release);
    return true;
}

template<typename T, size_t capacity>
bool LockFreeSPSCQueue<T, capacity>::pop_order_from_queue() {
    size_t head = mConsumer.head.load(std::memory_order_relaxed);
    if (head == mConsumer.tail_cached) {
        //queue appears empty
        //size_t tail = mProducer.tail.load(std::memory_order_acquire);
        mConsumer.tail_cached = mProducer.tail.load(std::memory_order_acquire);
        if (head == mConsumer.tail_cached) { return false; }
    }
    size_t slot_index = head & (capacity - 1);
    mConsumer.head.store((head+1)&(capacity-1), std::memory_order_release);
    return true;
}

template<typename T, size_t capacity>
int LockFreeSPSCQueue<T, capacity>::get_front_order_id() {
    if (queue_empty()) {
        return -1;
    }
    auto* buf = buffer();
    return buf[mConsumer.head & (capacity - 1)].order_id;
}

template<typename T, size_t capacity>
bool LockFreeSPSCQueue<T, capacity>::queue_full() {
    size_t head = mConsumer.head.load(std::memory_order_acquire);
    size_t tail = mProducer.tail.load(std::memory_order_acquire);
    return ((tail + 1) & (capacity - 1)) == head;
}

template<typename T, size_t capacity>
bool LockFreeSPSCQueue<T, capacity>::queue_empty() {
    size_t head = mConsumer.head.load(std::memory_order_seq_cst);
    size_t tail = mProducer.tail.load(std::memory_order_seq_cst);
    return head == tail;
}

template<typename T, size_t capacity>
int LockFreeSPSCQueue<T, capacity>::get_queue_current_size() {
    size_t head = mConsumer.head.load(std::memory_order_acquire);
    size_t tail = mProducer.tail.load(std::memory_order_acquire);
    return (tail - head) & (capacity - 1);
}
