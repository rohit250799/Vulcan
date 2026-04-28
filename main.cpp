#include <cassert>
#include <cstddef>
#include <iostream>
#include <iomanip>
#include <type_traits>

#include "vulcan/lock_free_spsc_queue.hpp"

int main() {
    auto* my_queue = LockFreeSPSCQueue<QueueOrder, 4>::create();
    std::cout << "The emptyness state of the queue is: " << my_queue->queue_empty() << " \n";
    my_queue->push_order_into_queue({100, 22.32, 232});
    my_queue->push_order_into_queue({101, 23, 49});
    std::cout << "The not emptyness state of the queue is: " << my_queue->queue_empty() << " \n";
    my_queue->pop_order_from_queue();
    return 0;
}
