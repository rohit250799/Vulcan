#include <cassert>
#include <cstddef>
#include <iostream>
#include <iomanip>
#include <type_traits>

#include "vulcan/lock_free_spsc_queue.hpp"

int main() {
    auto* my_queue = LockFreeSPSCQueue<QueueOrder, 256>::create();
    std::cout << "The emptyness state of the queue is: " << my_queue->queue_empty() << " \n";
    my_queue->push_order_into_queue({100, 22.32, 232});
    my_queue->push_order_into_queue({101, 23, 49});
    assert(my_queue->get_queue_current_size() == 2 && "Queue size assertion failed, it should be 2 \n");
    std::cout << "The not emptyness state of the queue is: " << my_queue->queue_empty() << " \n";
    const QueueOrder* popped_order = my_queue->pop_order_from_queue();
    std::cout << "The popped order id is: " << popped_order->order_id << " \n";
    assert(my_queue->get_queue_current_size() == 1 && "Queue size assertion failing \n");
    return 0;
}
