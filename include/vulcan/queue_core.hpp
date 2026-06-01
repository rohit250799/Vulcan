#include <array>
#include <pthread.h>
#include <cerrno>
#include <iostream>

void pinThreadToCore(pthread_t thread_id, int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    
    int set_affinity_mask_result = pthread_setaffinity_np(thread_id, sizeof(cpu_set_t), &cpuset);
    if (set_affinity_mask_result != 0) {
        std::cerr << "Error calling pthread_setaffinity_np for core: " << core_id << "has error: " << EXIT_FAILURE << " \n";
    }
}

bool populate_stack_buffer(std::array<QueueOrder, 1024>& my_queueOrder) { 
    for (size_t i = 0; i < 1024; ++i) {
        my_queueOrder[i] = {i, (i+1)*1.33, i+100};
    }
    assert(my_queueOrder[1023].order_id == 1023 && "Assertion failed: Stack buffer not populated correctly \n");
    return true;
}