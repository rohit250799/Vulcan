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
    my_queue->push_order_into_queue({101, 36.45, 49});
    my_queue->push_order_into_queue({102, 21.44, 49});
    my_queue->push_order_into_queue({103, 23.6, 49});
    my_queue->push_order_into_queue({104, 944.32, 49});
    
    assert(my_queue->get_queue_current_size() == 5 && "Queue size assertion failed, it should be 5 \n");
    std::cout << "The not emptyness state of the queue is: " << my_queue->queue_empty() << " \n";
    const QueueOrder* current_head_ptr = my_queue->peek_into_queue(0);
    my_queue->commit_pop_order_from_queue(current_head_ptr, 0);
    std::cout << "The current queue size is: " << my_queue->get_queue_current_size() << " \n";
    assert(my_queue->get_queue_current_size() == 4 && "Queue size assertion failing \n");
    std::cout << "The current front order is: " << my_queue->get_front_order_id() << " \n";
    return 0;
}
