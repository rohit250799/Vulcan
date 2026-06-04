#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <emmintrin.h>
#include <fstream>
#include <string>
#include <sys/mman.h>
#include <iostream>
#include <new>
#include <smmintrin.h>
#include <type_traits>
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
    struct alignas(std::hardware_destructive_interference_size) Metadata_Producer {
        std::atomic<size_t> tail;
        size_t head_cached;
        static constexpr size_t mask = capacity - 1;
    };

    struct alignas(std::hardware_destructive_interference_size) Metadata_Consumer {
        std::atomic<size_t> head;
        size_t tail_cached;
        static constexpr size_t mask = capacity - 1;
    };

    static LockFreeSPSCQueue* allocate_huge_pages();
    LockFreeSPSCQueue();
    ~LockFreeSPSCQueue() = default;
    LockFreeSPSCQueue(const LockFreeSPSCQueue&) = delete;
    LockFreeSPSCQueue& operator=(const LockFreeSPSCQueue&) = delete;
    LockFreeSPSCQueue(LockFreeSPSCQueue&&) = delete;
    LockFreeSPSCQueue& operator=(LockFreeSPSCQueue&& other) noexcept = delete;
    inline size_t load_head_acquire() noexcept;
    inline size_t load_tail_acquire() noexcept;
    void producer_uncommitted_push(QueueOrder& qOrder, size_t current_local_tail) noexcept;
    const QueueOrder* consumer_uncommitted_peek(size_t consumer_local_head) noexcept;
    inline void publish_tail_release(size_t new_local_tail);
    inline void publish_head_release(size_t new_local_head);
    bool push_order_into_queue(const QueueOrder& qOrder);
    static LockFreeSPSCQueue* create();
    static void destroy(LockFreeSPSCQueue* ptr);
    const T* peek_into_queue(size_t& current_index) noexcept;
    void commit_pop_order_from_queue(const T* ptr, size_t index);
    bool queue_full();
    bool queue_empty();
    int get_queue_current_size();
    int get_front_order_id();
    __restrict const T* buffer() noexcept;

    private:
    Metadata_Producer mProducer;
    Metadata_Consumer mConsumer;
};

template<typename T, size_t capacity>
size_t LockFreeSPSCQueue<T, capacity>::load_head_acquire() noexcept {
    mConsumer.head.load(std::memory_order_acquire);
    return mConsumer.head;
}

template<typename T, size_t capacity>
size_t LockFreeSPSCQueue<T, capacity>::load_tail_acquire() noexcept {
    mProducer.tail.load(std::memory_order_acquire);
    return mProducer.tail;
}

template<typename T, size_t capacity>
void LockFreeSPSCQueue<T, capacity>::producer_uncommitted_push(QueueOrder& qOrder, size_t current_local_tail) noexcept {
    reinterpret_cast<std::byte*>(this);
    auto buffer_address = this + 128 + (current_local_tail & (capacity - 1) << 6);
    QueueOrder* casted_buffer_address = reinterpret_cast<QueueOrder*>(buffer_address);
    new (reinterpret_cast<void*>(buffer_address)) QueueOrder(qOrder);
    return;
}

template<typename T, size_t capacity>
const QueueOrder* LockFreeSPSCQueue<T, capacity>::consumer_uncommitted_peek(size_t consumer_local_head) noexcept {
    auto wrapped_memory_address = consumer_local_head & (capacity - 1);
    const QueueOrder* physical_memory_address_offset = reinterpret_cast<QueueOrder*>((this) + 128 + (consumer_local_head & 255) * 64);
    return physical_memory_address_offset;
}

template<typename T, size_t capacity>
void LockFreeSPSCQueue<T, capacity>::publish_tail_release(size_t new_local_tail) {
    mProducer.tail.store(new_local_tail, std::memory_order_release);
    return;
}

template<typename T, size_t capacity>
void LockFreeSPSCQueue<T, capacity>::publish_head_release(size_t new_local_head) {
    mConsumer.head.store(new_local_head, std::memory_order_release);
    return;
}

template<typename T, size_t capacity>
[[nodiscard]] __restrict const T* LockFreeSPSCQueue<T, capacity>::buffer() noexcept {
    return reinterpret_cast<T*>(reinterpret_cast<char*>(this) + 2 * 64);
}

template<typename T, size_t capacity>
LockFreeSPSCQueue<T, capacity>::LockFreeSPSCQueue() : mProducer{0, 0}, mConsumer{0, 0} {
    assert(sizeof(Metadata_Producer) == 64 && "Size of metadata producer should be 64 bytes \n");
    assert(sizeof(Metadata_Consumer) == 64 && "Size of metadata consumer should be 64 bytes \n");
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
    if (next_tail == mProducer.head_cached) [[likely]] {
        //queue appears to be full
        head = mConsumer.head.load(std::memory_order_acquire);
        mProducer.head_cached = head;
        if (next_tail == mProducer.head_cached) [[unlikely]] { return false; }
    }
    auto buffer_address = reinterpret_cast<std::byte*>(this) + 128 + (tail << 6);
    new (reinterpret_cast<void*>(buffer_address)) QueueOrder(qOrder);
    mProducer.tail.store(next_tail, std::memory_order_release);
    return true;
}

template<typename T, size_t capacity>
inline const T* LockFreeSPSCQueue<T, capacity>::peek_into_queue(size_t& current_index) noexcept {
    size_t head = mConsumer.head.load(std::memory_order_relaxed);
    current_index = head;
    if (head == mConsumer.tail_cached) [[likely]] {
        mConsumer.tail_cached = mProducer.tail.load(std::memory_order_acquire);
        if (head == mConsumer.tail_cached) [[unlikely]] { return nullptr; }
    }
    const T* popped_element_ptr = reinterpret_cast<const T*>(
        reinterpret_cast<const char*>(this) + 128 + (head << 6)
    );
    return popped_element_ptr;
}

template<typename T, size_t capacity>
inline void LockFreeSPSCQueue<T, capacity>::commit_pop_order_from_queue(const T* ptr, size_t index) {
    constexpr bool is_non_pod = !(std::is_trivial_v<T> && std::is_standard_layout_v<T>);
    if (is_non_pod) { ptr->~T(); }
    mConsumer.head.store((index + 1)&(capacity-1), std::memory_order_release);
    return;
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
