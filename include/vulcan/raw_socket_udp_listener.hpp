#include <cstddef>

class Zero_Copy_UDP_Listener {
    private:
        int sockfd;
        void* mmap_ring;
        std::size_t ring_size;
        int target_core;
        
        void set_cpu_affinity(); // pinning thread to prevent OS from migrating it
        void enable_busy_polling(); // agressive driver polling to bypass interrupts
        void setup_mmap_ring(); // Map the NIC Rx queue directly to userspace
    
    public:
        Zero_Copy_UDP_Listener(int core_id);
        ~Zero_Copy_UDP_Listener();
        
        void init_socket(); //instantiating raw_socket
        
        void poll_loop(); // critical hot-path, no syscalls allowed here
};

