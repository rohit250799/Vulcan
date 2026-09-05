# Vulcan task runner. Install: `sudo dnf install just` (or `cargo install just`).
# Usage examples:
#   just build            -> release build (default)
#   just build debug      -> debug build
#   just test             -> run all tests (debug)
#   just valgrind         -> leak check on main binary
#   just bench            -> run the benchmark
# Run `just --list` to see every recipe.

set shell := ["bash", "-uc"]

# ---- configure / build -------------------------------------------------

configure preset="release":
    cmake --preset {{preset}}

build preset="release": (configure preset)
    cmake --build --preset {{preset}}

debug: (build "debug")
release: (build "release")
bench-build: (build "benchmark")

# ---- run -----------------------------------------------------------------

run preset="release": (build preset)
    sudo build/{{preset}}/src/vulcan

bench: (build "benchmark")
    cmake --build --preset benchmark --target run_benchmark

# ---- tests -----------------------------------------------------------------

test preset="debug": (build preset)
    ctest --preset {{preset}} --output-on-failure

test-one name preset="debug": (build preset)
    ctest --preset {{preset}} -R {{name}} --output-on-failure

# ---- debugging -----------------------------------------------------------------

find-error preset="debug": (build preset)
    sudo gdb -batch -ex run -ex backtrace -ex quit build/{{preset}}/src/vulcan

find-bench-error preset="debug": (build preset)
    gdb -batch -ex run -ex backtrace -ex quit build/{{preset}}/benchmarks/vulcan_benchmark

valgrind preset="debug": (build preset)
    sudo valgrind --leak-check=full --show-leak-kinds=all build/{{preset}}/src/vulcan

valgrind-bench preset="debug": (build preset)
    valgrind --leak-check=full build/{{preset}}/benchmarks/vulcan_benchmark

helgrind preset="debug": (build preset)
    valgrind --tool=helgrind build/{{preset}}/src/vulcan

# ---- performance analysis -----------------------------------------------------------------

analyze-benchmark: (build "benchmark")
    mkdir -p benchmarks/results
    sudo perf stat -x, -e cycles,instructions,branches,branch-misses,cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,dTLB-loads,dTLB-load-misses,stalled-cycles-frontend,stalled-cycles-backend,context-switches,cpu-migrations,page-faults -o benchmarks/results/unified_stats_benchmark.csv build/benchmark/benchmarks/vulcan_benchmark
    column -s, -t < benchmarks/results/unified_stats_benchmark.csv

check-latency preset="release": (build preset)
    sudo perf mem record -d build/{{preset}}/src/vulcan
    sudo perf mem report --stdio --verbose

check-false-sharing preset="release": (build preset)
    sudo perf c2c record -o c2c.data build/{{preset}}/src/vulcan
    sudo perf c2c report --stdio -i c2c.data

machine preset="debug": (build preset)
    objdump -D build/{{preset}}/src/CMakeFiles/vulcan.dir/main.cpp.o

info preset="release": (configure preset)
    cmake -LAH build/{{preset}}

# ---- housekeeping -----------------------------------------------------------------

format: (configure "debug")
    cmake --build --preset debug --target format

clean:
    rm -rf build

clean-preset preset:
    rm -rf build/{{preset}}
