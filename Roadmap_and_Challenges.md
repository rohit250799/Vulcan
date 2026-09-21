# Vulcan — Roadmap & Challenges

This document tracks Vulcan's current focus, resolved challenges, and known limitations. For architecture and benchmarking detail, see `Technical_Documentation.md`.

---

## Currently Working On

1. **Lock-free SPSC circular queue** — implementation described in `Technical_Documentation.md`
2. **Raw Socket Zero-Copy UDP Listener** — implementation described in `Technical_Documentation.md`

## Current Problem

- The total number of system calls being made is not yet fully confirmed in the hot path.

## Next Focus

- Verifying the total number of system calls being made in the hot path
- Replacing polling with busy polling

---

## Solved Challenges

1. **L1-D cache sizing**: The AMD Zen 3 chip has a 32KB L1 Data Cache per core. An SPSC queue with 1024 capacity would need 1024 × 64 = 64KB, exceeding L1. Reduced capacity to 256 (16KB), which also satisfies the power-of-2 capacity assertion. *(Solved)*
2. **memset as a cold-path fix**: Using `memset(obj, 0x00, sizeof(*obj))` warms physical memory but doesn't address Store-to-Load Forwarding conflicts when transitioning from buffer initialization to the high-speed matching loop. *(Solved)*
3. **Non-temporal stores**: Replaced the `memset` call in the create function with non-temporal store intrinsics (e.g. `_mm_stream_si128`), working through a C++ type-safety violation along the way. *(Solved)*
4. **Physical memory fragmentation**: `mmap` intermittently stopped working even with a huge page successfully allocated earlier. On AMD Zen 3, a 2MB Huge Page requires 512 *consecutive* 4KB physical page frames — as the system ran, scattered 4KB allocations from user-space and kernel tasks left no contiguous 2MB gaps. Resolved by a system reboot. *(Solved)*
5. **Write-After-Read hazard in `pop`**: A single monolithic pop function returning a `const` pointer before updating the head created a race — if the head index updates before the consumer finishes reading, the producer could overwrite the slot mid-read. Fixed by splitting `pop` into Access and Release phases. *(Solved)*
6. **NUMA pinning**: On multi-socket architectures, physical memory access costs vary by node, breaking deterministic behavior. Needs `numactl --membind` for hardware NUMA pinning.
7. **Cycles-per-element optimization**: Started at 17 cycles/element in both threads, with IPC of 0.28 (target 1.5+) and branch misses at 1.28%. Partially solved — now at 6 cycles/element in both threads (later improved further, see README benchmark table).
8. **Core/CCD placement**: Producer pinned to CPU 0 / Core 0, consumer to CPU 2 / Core 1 — needed to confirm both lie on the same Core Chiplet Die (CCD) for fast inter-core communication. Confirmed: on AMD Ryzen 5000 (Zen 3), each CCD holds 8 cores with logical cores 0–7 mapped sequentially to CCD #0, so CPU 0 and 2 always share a CCD. *(Solved)*
9. **Deterministic memory binding**: Needed `mbind` (from `numaif.h`) in the benchmark file, which required installing `libnuma-dev` first. *(Solved)*
10. **Testing spin-wait operations**: A single-threaded test of popping from an empty queue would deadlock, since `pop` spin-waits until an element is available. Solved with a minimal deterministic dual-thread test harness using a separate consumer thread function. *(Solved)*
11. **Zero-copy UDP kernel bypass**: Achieved using `PF_PACKET` with `PACKET_MMAP` for a size-configurable circular buffer mapped in user space, avoiding a syscall on most reads. Required confirming NAPI support on the NIC driver and enabling it; automated via a bash script. *(Done)*

## Known Limitations

12. **NIC hardware limitation — unsolvable on current hardware**: The current NIC driver (Realtek Wi-Fi 6) does not support Threaded NAPI, a feature relevant to high-performance low-latency trading environments. Wi-Fi drivers generally don't implement it — wireless packet rates are much lower and driver architecture differs, so the feature wasn't designed for wireless. This cannot be enabled in software.
    - **Path forward**: only solvable with a hardware upgrade — an Intel 10GbE NIC with a wired Ethernet connection and proper kernel tuning.

---

## Roadmap

- [ ] Confirm and minimize total syscalls in the hot path
- [ ] Replace polling with busy polling
- [ ] Continue reducing cycles-per-element toward single digits across both threads
- [ ] Hardware upgrade: Intel 10GbE NIC + wired Ethernet for Threaded NAPI support
- [ ] Publish full Build and Debug guide
