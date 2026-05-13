#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <emmintrin.h>
#include <exception>
#include <fstream>
#include <string>
#include <sys/mman.h>
#include <iostream>
#include <iterator>
#include <limits>
#include <new>
#include <stdatomic.h>
#include <smmintrin.h>
#include <xmmintrin.h>

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
        std::atomic<size_t> tail;
        size_t head_cached;
        static constexpr size_t mask = capacity - 1;
        char padding[64 - ( (sizeof(uint64_t) * 2) % 64 )];
    };

    struct alignas(64) Metadata_Consumer {
        std::atomic<size_t> head;
        size_t tail_cached;
        static constexpr size_t mask = capacity - 1;
        char padding[64 - ( (sizeof(uint64_t) * 2) % 64 )];
    };

    static LockFreeSPSCQueue* allocate_huge_pages();
    LockFreeSPSCQueue();
    ~LockFreeSPSCQueue() = default;
    LockFreeSPSCQueue(const LockFreeSPSCQueue&) = delete;
    LockFreeSPSCQueue& operator=(const LockFreeSPSCQueue&) = delete;
    //bool pre_allocate_huge_page_pool();
    bool push_order_into_queue(const QueueOrder& qOrder);
    static LockFreeSPSCQueue* create();
    static void destroy(LockFreeSPSCQueue* ptr);
    const T* pop_order_from_queue();
    bool queue_full();
    bool queue_empty();
    int get_queue_current_size();
    int get_front_order_id();
    __restrict T* buffer();

    private:
    Metadata_Producer mProducer;
    Metadata_Consumer mConsumer;
};

template<typename T, size_t capacity>
__restrict T* LockFreeSPSCQueue<T, capacity>::buffer() {
    return reinterpret_cast<T*>(reinterpret_cast<char*>(this) + 2 * 64);
}

template<typename T, size_t capacity>
LockFreeSPSCQueue<T, capacity>::LockFreeSPSCQueue() : mProducer{0, 0}, mConsumer{0, 0} {
    assert(sizeof(Metadata_Producer) == 64 && "Size of metadata producer should be 64 bytes \n");
    assert(sizeof(Metadata_Consumer) == 64 && "Size of metadata consumer should be 64 bytes \n");
    //char* offset_ptr = (char*)obj + 128;
    assert(reinterpret_cast<uintptr_t>(this)%64 == 0 && "Assertion failed: Non functional core.. \n");
    assert(mProducer.mask == mConsumer.mask && "Mask in both producer and consumer threads should be equal \n");
    assert(sizeof(QueueOrder) % 64 == 0 && "QueueOrders should be 64 bytes in size \n");
}

//static factory method for class is the buffer allocation
template<typename T, size_t capacity>
LockFreeSPSCQueue<T, capacity>* LockFreeSPSCQueue<T, capacity>::create() {
    const std::string required_proc_file_path = "/proc/sys/vm/nr_hugepages";
    std::ifstream proc_file(required_proc_file_path);
    if (!proc_file.is_open()) {
        std::cerr << "Could not open file from path: " << required_proc_file_path << " \n";
        std::exit(EXIT_FAILURE);
    }
    int current_huge_pages = proc_file.get() - '0';
    std::cout << "The huge pages value is: " << current_huge_pages << " \n";
    if (current_huge_pages != 1) {
        std::cerr << "Huge pages still not 1. Terminating the program \n";
        std::exit(EXIT_FAILURE);
    }
    proc_file.close();
    const size_t boundary = 2 * 1024 * 1024;
    size_t base_size_of_class = sizeof(LockFreeSPSCQueue<T, capacity>);
    size_t rounded_size = (base_size_of_class + (boundary - 1)) & ~(boundary - 1);
    auto* mem_ptr = mmap(NULL, rounded_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_POPULATE, -1, 0);
    if (mem_ptr == MAP_FAILED) {
        perror("mmap");
        std::exit(EXIT_FAILURE);
    }
    LockFreeSPSCQueue* obj = new (mem_ptr) LockFreeSPSCQueue();
    char* offset_ptr = (char*)obj + 128;
    size_t L1_stride = 2048;
    assert(((uintptr_t)offset_ptr % L1_stride) != 0 && "Buffer offset should not be a multiple of L1 critical stride \n");
    __m128i* simd_obj_ptr = reinterpret_cast<__m128i*>(mem_ptr);
    __m128i zero_vector = _mm_setzero_si128();
    static_cast<char*>(mem_ptr)[0] = 0;

    for(size_t i = 0; i < (2*1024*1024) / 16; ++i) {
        _mm_stream_si128(simd_obj_ptr + i, zero_vector);
    }
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
const T* LockFreeSPSCQueue<T, capacity>::pop_order_from_queue() {
    size_t head = mConsumer.head.load(std::memory_order_relaxed);
    if (head == mConsumer.tail_cached) {
        //queue appears empty
        mConsumer.tail_cached = mProducer.tail.load(std::memory_order_acquire);
        if (head == mConsumer.tail_cached) { return nullptr; }
    }
    const T* popped_element = reinterpret_cast<const T*>(
        reinterpret_cast<const char*>(this) + 128 + ((head) & (capacity - 1)) * 64
    );
    mConsumer.head.store((head+1)&(capacity-1), std::memory_order_release);
    return popped_element;
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
