#include "vulcan/raw_socket_udp_listener"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <sys/mman.h>
#include <sys/socket.h>

Zero_Copy_UDP_Listener::Zero_Copy_UDP_Listener(int core_id) {
    std::cout << "Setting up a zero copy UDP listener";
}

Zero_Copy_UDP_Listener::init_socket() {
    sockfd = socket(AF_PACKET, SOCK_RAW, IPROTO_UDP);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }
    setup_mmap_ring();
    return;
}

void Zero_Copy_UDP_Listener::setup_mmap_ring() {
    const size_t memory_space = 16000000;
    auto* mem_ptr = mmap(NULL, memory_space, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_HUGETLB | MAP_POPULATE, sockfd, 0);
    if (mem_ptr == MAP_FAILED)
        err(EXIT_FAILURE, "mmap");
    return;
}