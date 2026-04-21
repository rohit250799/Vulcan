#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <limits>
#include <new>

struct alignas(64) QueueOrder {
    uint64_t order_id;
    double price;
    uint32_t quantity;
};

struct alignas(64) Metadata_Producer {
    std::atomic<uint64_t> tail = 0;
    uint64_t head_cached;
};

struct alignas(64) Metadata_Consumer {
    std::atomic<uint64_t> head = 0;
    uint64_t tail_cached;
};

class LockFreeSPSCQueue {
    public:
    LockFreeSPSCQueue(int capacity);
    ~LockFreeSPSCQueue() = default;
    LockFreeSPSCQueue(const LockFreeSPSCQueue&) = delete;
    LockFreeSPSCQueue& operator=(const LockFreeSPSCQueue&) = delete;
    bool push_order_into_queue(const QueueOrder& qOrder);
    static LockFreeSPSCQueue* create(int capacity);
    static void destroy(LockFreeSPSCQueue* ptr);
    bool pop_order_from_queue();
    bool queue_full();
    bool queue_empty();
    void display_all_orders_in_queue();
    int get_queue_current_size();
    int get_front_order_id();

    private:
    Metadata_Producer mProducer;
    Metadata_Consumer mConsumer;
    int capacity;
    static constexpr int L = 4;
    QueueOrder q[];
};