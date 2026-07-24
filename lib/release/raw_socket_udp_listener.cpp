#include "vulcan/raw_socket_udp_listener"
#include <asm-generic/socket.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <sys/mman.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <sys/user.h>

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
    struct tpacket_req req = {4096, 4, 2048, 8};
    setsockopt(sockfd, SOL_PACKET, PACKET_RX_RING, (void*) &req, sizeof(req));
    mmap_rx_ring = mmap(0, memory_space, PROT_READ | PROT_WRITE, MAP_SHARED, sockfd, 0);
    if (mmap_rx_ring == MAP_FAILED)
        err(EXIT_FAILURE, "mmap");
    return;
}

void Zero_Copy_UDP_Listener::poll_loop() {
    
}