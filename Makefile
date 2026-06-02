# ============================================================================
# Production Makefile for Vulcan Core (Quant Firm Grade)
# With Separate Debug and Release Configurations
# ============================================================================

CXX = g++

# ============================================================================
# Configuration Selection
# ============================================================================
# Usage: 
#   make config=release (default) - production build with optimizations
#   make config=debug              - debug build with symbols, no optimizations
#   make config=benchmark          - optimized but with minimal debug info for perf
# ============================================================================

CONFIG ?= release

# ============================================================================
# Compiler Flags per Configuration
# ============================================================================

# Base flags (common to all)
BASE_CXXFLAGS = -std=c++20 -Iinclude -Wall -Wextra -Wpedantic -pthread
BASE_LDFLAGS = -lpthread

# Release configuration (production - maximum performance)
ifeq ($(CONFIG),release)
    CXXFLAGS = $(BASE_CXXFLAGS) -O3 -mcx16 -march=znver3 -DNDEBUG -flto
    LDFLAGS = $(BASE_LDFLAGS) -flto
    BUILD_SUFFIX = release
    STRIP_SYMBOLS = yes
endif

# Debug configuration (full debugging, no optimizations)
ifeq ($(CONFIG),debug)
    CXXFLAGS = $(BASE_CXXFLAGS) -O0 -g3 -gdwarf-4 -ggdb -DDEBUG -fno-omit-frame-pointer
    CXXFLAGS += -fno-inline -fno-eliminate-unused-debug-types -fno-optimize-sibling-calls
    LDFLAGS = $(BASE_LDFLAGS)
    BUILD_SUFFIX = debug
    STRIP_SYMBOLS = no
endif

# Benchmark configuration (optimized but with debug symbols for perf)
ifeq ($(CONFIG),benchmark)
    CXXFLAGS = $(BASE_CXXFLAGS) -O3 -mcx16 -march=znver3 -DNDEBUG -g -fno-omit-frame-pointer
    LDFLAGS = $(BASE_LDFLAGS)
    BUILD_SUFFIX = benchmark
    STRIP_SYMBOLS = no
endif

# ============================================================================
# Directories (versioned by configuration to avoid mixing)
# ============================================================================
ROOT_DIR := $(shell pwd)
SRC_DIR = $(ROOT_DIR)/src

# Production directories (versioned)
BUILD_DIR = $(ROOT_DIR)/build/$(BUILD_SUFFIX)
BIN_DIR = $(ROOT_DIR)/bin/$(BUILD_SUFFIX)
LIB_DIR = $(ROOT_DIR)/lib/$(BUILD_SUFFIX)

# Benchmark directories (versioned)
BENCHMARK_DIR = $(ROOT_DIR)/benchmarks
BENCHMARK_BUILD_DIR = $(BENCHMARK_DIR)/build/$(BUILD_SUFFIX)
BENCHMARK_BIN_DIR = $(BENCHMARK_DIR)/bin/$(BUILD_SUFFIX)
BENCHMARK_RESULTS_DIR = $(BENCHMARK_DIR)/results

# ============================================================================
# Sources and Objects
# ============================================================================
SHARED_SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
SHARED_HEADERS = $(wildcard $(SRC_DIR)/*.h) $(wildcard $(SRC_DIR)/*.hpp)
SHARED_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SHARED_SOURCES))

MAIN_SRC = main.cpp
MAIN_OBJ = $(BUILD_DIR)/main.o

LIBRARY = $(LIB_DIR)/libhft.a
TARGET = $(BIN_DIR)/vulcan

# Benchmark sources
BENCHMARK_SRC = $(BENCHMARK_DIR)/benchmark.cpp
BENCHMARK_OBJECTS = $(BENCHMARK_BUILD_DIR)/benchmark.o \
                    $(patsubst $(SRC_DIR)/%.cpp,$(BENCHMARK_BUILD_DIR)/%.o,$(SHARED_SOURCES))
BENCHMARK_TARGET = $(BENCHMARK_BIN_DIR)/benchmark

# Test sources
TEST_SRC = $(wildcard tests/*.cpp)
TEST_OBJ = $(patsubst tests/%.cpp,$(BUILD_DIR)/%.test.o,$(TEST_SRC))
TEST_BIN = $(BIN_DIR)/tests_runner

# ============================================================================
# Default Target (release build)
# ============================================================================
.DEFAULT_GOAL := all

# ============================================================================
# Production Build
# ============================================================================

all: directories library program

directories:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR) $(BENCHMARK_RESULTS_DIR)
	@echo "Building with CONFIG=$(CONFIG)"
	@echo "Build directory: $(BUILD_DIR)"
	@echo "Binary directory: $(BIN_DIR)"

library: $(LIBRARY)

$(LIBRARY): $(SHARED_OBJECTS)
	ar rcs $@ $^

program: $(TARGET)

$(TARGET): $(MAIN_OBJ) $(LIBRARY)
	$(CXX) $(CXXFLAGS) -o $@ $^ -L$(LIB_DIR) -lhft $(LDFLAGS)
	@if [ "$(STRIP_SYMBOLS)" = "yes" ]; then \
		echo "Stripping symbols from $(TARGET)"; \
		strip --strip-all $(TARGET); \
	fi
	@echo "Production binary built: $(TARGET)"

$(BUILD_DIR)/main.o: $(MAIN_SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(SHARED_HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================================================
# Benchmark Build (completely isolated per config)
# ============================================================================

benchmark: $(BENCHMARK_TARGET)

$(BENCHMARK_TARGET): $(BENCHMARK_OBJECTS)
	@mkdir -p $(BENCHMARK_BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@if [ "$(STRIP_SYMBOLS)" = "yes" ]; then \
		echo "Stripping symbols from $(BENCHMARK_TARGET)"; \
		strip --strip-all $(BENCHMARK_TARGET); \
	fi
	@echo "Benchmark built: $(BENCHMARK_TARGET)"

$(BENCHMARK_BUILD_DIR)/benchmark.o: $(BENCHMARK_SRC) $(SHARED_HEADERS)
	@mkdir -p $(BENCHMARK_BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Iinclude -c $< -o $@

$(BENCHMARK_BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(SHARED_HEADERS)
	@mkdir -p $(BENCHMARK_BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Iinclude -c $< -o $@

# ============================================================================
# Root-Level Symlink for Convenience (points to current config's benchmark)
# ============================================================================

benchmark-link: benchmark
	@ln -sf $(BENCHMARK_TARGET) $(ROOT_DIR)/benchmark
	@echo "Symlink created: ./benchmark -> $(BENCHMARK_TARGET)"

# ============================================================================
# Benchmark Analysis Commands (use appropriate config)
# ============================================================================

analyze_benchmark_performance: benchmark
	@echo "=== Initializing Telemetry Directory ==="
	@mkdir -p $(BENCHMARK_RESULTS_DIR)

	@echo "=== Executing Unified PMC Read ==="

	sudo perf stat -x, \
		-e cycles,\
instructions,\
branches,\
branch-misses,\
cache-references,\
cache-misses,\
L1-dcache-loads,\
L1-dcache-load-misses,\
dTLB-loads,\
dTLB-load-misses,\
stalled-cycles-frontend,\
stalled-cycles-backend,\
context-switches,\
cpu-migrations,\
page-faults \
		-o $(BENCHMARK_RESULTS_DIR)/unified_stats_$(CONFIG).csv \
		$(BENCHMARK_TARGET)

	@echo "=== Performance Ledger Generated ==="
	@column -s, -t < $(BENCHMARK_RESULTS_DIR)/unified_stats_$(CONFIG).csv

check_benchmark_latency: benchmark
	@mkdir -p $(BENCHMARK_RESULTS_DIR)
	sudo perf mem record -d $(BENCHMARK_TARGET) -o $(BENCHMARK_RESULTS_DIR)/latency_$(CONFIG).data
	sudo perf mem report --stdio --verbose > $(BENCHMARK_RESULTS_DIR)/latency_report_$(CONFIG).txt
	sudo perf c2c record -o $(BENCHMARK_RESULTS_DIR)/c2c_$(CONFIG).data $(BENCHMARK_TARGET)
	sudo perf c2c report --stdio > $(BENCHMARK_RESULTS_DIR)/c2c_report_$(CONFIG).txt

find_benchmark_error: benchmark-link
	gdb -ex "run" -ex "backtrace" -ex "quit" ./benchmark

run_benchmark: benchmark-link
	./benchmark

# ============================================================================
# Debug-Specific Targets (convenience wrappers)
# ============================================================================

debug:
	@$(MAKE) config=debug all

debug-benchmark:
	@$(MAKE) config=debug benchmark

debug-run: debug-benchmark
	@$(MAKE) config=debug run_benchmark

debug-analyze:
	@$(MAKE) config=debug analyze_benchmark_performance

# ============================================================================
# Release-Specific Targets (explicit)
# ============================================================================

release:
	@$(MAKE) config=release all

release-benchmark:
	@$(MAKE) config=release benchmark

release-run: release-benchmark
	@$(MAKE) config=release run_benchmark

# ============================================================================
# Benchmark-Specific Config (optimized + symbols for perf)
# ============================================================================

benchmark-config:
	@$(MAKE) config=benchmark benchmark

benchmark-run: benchmark-config
	@$(MAKE) config=benchmark run_benchmark

# ============================================================================
# Tests
# ============================================================================

tests: $(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(LIBRARY)
	$(CXX) $(CXXFLAGS) -o $@ $^ -L$(LIB_DIR) -lhft $(LDFLAGS)

run_tests: $(TEST_BIN)
	./$(TEST_BIN)

# ============================================================================
# Production Analysis (main binary)
# ============================================================================

analyze_performance: program
	perf stat -e cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,dTLB-load-misses $(TARGET)
	perf stat -e cycles,instructions,cache-misses,branch-misses $(TARGET)
	sudo perf mem record -d $(TARGET) && sudo perf mem report -f --stdio --sort=sym --percent-limit=0

check_latency: program
	sudo perf mem record -d $(TARGET)
	sudo perf mem report --stdio --verbose
	sudo perf c2c report $(TARGET)

find_error: program
	gdb -ex "run" -ex "backtrace" -ex "quit" $(TARGET)

# ============================================================================
# Utilities
# ============================================================================

run: program
	./$(TARGET)

machine:
	objdump -D $(BUILD_DIR)/main.o

# ============================================================================
# Clean Targets
# ============================================================================

clean:
	@echo "Cleaning all build artifacts..."
	rm -rf $(ROOT_DIR)/build
	rm -rf $(ROOT_DIR)/bin
	rm -rf $(ROOT_DIR)/lib
	rm -rf $(BENCHMARK_BUILD_DIR)
	rm -rf $(BENCHMARK_BIN_DIR)
	rm -f $(ROOT_DIR)/benchmark
	@echo "Clean complete"

clean-release:
	@echo "Cleaning release build..."
	rm -rf $(ROOT_DIR)/build/release
	rm -rf $(ROOT_DIR)/bin/release
	rm -rf $(ROOT_DIR)/lib/release
	rm -rf $(BENCHMARK_DIR)/build/release
	rm -rf $(BENCHMARK_DIR)/bin/release

clean-debug:
	@echo "Cleaning debug build..."
	rm -rf $(ROOT_DIR)/build/debug
	rm -rf $(ROOT_DIR)/bin/debug
	rm -rf $(ROOT_DIR)/lib/debug
	rm -rf $(BENCHMARK_DIR)/build/debug
	rm -rf $(BENCHMARK_DIR)/bin/debug

clean-benchmark:
	@echo "Cleaning benchmark artifacts..."
	rm -rf $(BENCHMARK_RESULTS_DIR)

clean-all: clean

# ============================================================================
# Info / Status
# ============================================================================

info:
	@echo "=== Build Configuration ==="
	@echo "Current CONFIG: $(CONFIG)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "Build dir: $(BUILD_DIR)"
	@echo "Binary dir: $(BIN_DIR)"
	@echo ""
	@echo "Available targets:"
	@echo "  make                 - Release build (default)"
	@echo "  make debug           - Debug build (-O0 -g3)"
	@echo "  make release         - Release build (-O3)"
	@echo "  make benchmark-config - Benchmark build (-O3 with symbols)"
	@echo "  make debug-benchmark - Debug benchmark"
	@echo "  make debug-run       - Run debug benchmark"
	@echo "  make run_benchmark   - Run benchmark (current config)"
	@echo "  make clean           - Clean everything"

# ============================================================================
# Phony declarations
# ============================================================================

.PHONY: all directories library program benchmark
.PHONY: analyze_benchmark_performance check_benchmark_latency find_benchmark_error run_benchmark
.PHONY: tests run_tests analyze_performance check_latency find_error
.PHONY: run machine clean clean-release clean-debug clean-benchmark clean-all
.PHONY: debug debug-benchmark debug-run debug-analyze
.PHONY: release release-benchmark release-run
.PHONY: benchmark-config benchmark-run
.PHONY: benchmark-link info