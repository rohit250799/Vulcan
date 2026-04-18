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

class LockFreeSPSCQueue {
    public:
    LockFreeSPSCQueue(int capacity);
    ~LockFreeSPSCQueue();
    LockFreeSPSCQueue(const LockFreeSPSCQueue&) = delete;
    LockFreeSPSCQueue& operator=(const LockFreeSPSCQueue&) = delete;
    bool push_order_into_queue(const QueueOrder& qOrder);
    bool pop_order_from_queue();
    bool queue_full();
    bool queue_empty();
    void display_all_orders_in_queue();
    int get_queue_current_size();
    int get_front_order_id();

    private:
    alignas(64) std::atomic<uint64_t> head = 0;
    alignas(64) std::atomic<uint64_t> tail = 0;
    uint64_t head_cached;
    uint64_t tail_cached;
    QueueOrder* m_order = nullptr;
    std::byte* raw_buffer = nullptr;
    int capacity;

    //int mIndexEnd; // at this, wrap indices around to 0
};