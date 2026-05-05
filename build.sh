echo "Hello, Rohit!"
date

# Example using <int, 1024> - replace with your actual types
CLASS_SIZE=$(printf '#include "include/vulcan/lock_free_spsc_queue.hpp"\n#include <iostream>\nint main(){std::cout << sizeof(LockFreeSPSCQueue<QueueOrder, 4>);}' | g++ -x c++ -I./include -o size_check - && ./size_check && rm size_check)
echo "The size is: $CLASS_SIZE"

#memory calculation logic:
# CALCULATED_REQUIREMENT=$((CLASS_SIZE / 2097152))
# echo "The calculated requirement is: $CALCULATED_REQUIREMENT"

modify_kernel_parameter_hugepages() {
    if sudo sysctl -w vm.nr_hugepages=1; then
        echo "NR Hugepages modification successful. Memory reserved = 1 huge page or 2mb";
    else
        echo "Hugepages modification not successful. Please try again";
    fi
}

modify_kernel_parameter_hugepages
