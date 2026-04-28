# Vulcan
A Low-Latency Trading Engine (Tick-to-Trade system) built from ground up to run on Linux and specific AMD Ryzen 5600H. It acts as a 
deterministic pipeline. On completion, Vulcan will do 4 specific tasks:

1. **Feed Arbitration**: Receiving multiple copies of market data (UDP) and picking the fastest one using Zero-Copy techniques
2. **LOB (Limit Order Book) Management**: Maintaining a real-time map of every "Buy" and "Sell" order in the market with O(1) complexity
3. **Risk Checking**: Validating that a trade won't bankrupt the firm in under 100 nanoseconds.
4. **Order Entry**: Formatting a "Buy" or "Sell" instruction into a binary protocol (like FIX/SBE) and blasting it back to the exchange.

Commands to test the Engine(the order of commands is to be always maintained):
1. **make clean** - to start fresh from beginning
2. **make** - build
3. **make run_tests** - run the tests
4. **make analyze_performance** - get performance report on terminal
5. **make machine** - get disassembler to produce machine code

To test performance, from the root directory - type these commands in the terminal
1. **perf stat -e cycles,instructions,cache-misses,branch-misses ./bin/vulcan**
2. **perf stat -e cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses ./bin/vulcan**

Currently working on:
1. **Lock-free SPSC circular Queue**