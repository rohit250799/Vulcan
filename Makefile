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
BASE_LDFLAGS = -lpthread -lnuma

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
    CXXFLAGS = $(BASE_CXXFLAGS) -O3 -mcx16 -march=native -DNDEBUG -g -fno-omit-frame-pointer
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

# ============================================================================
# Manual Test Harness (No Frameworks)
# ============================================================================

TEST_UNIT_DIR = tests/unit
TEST_INTEG_DIR = tests/integration
TEST_BENCH_DIR = tests/benchmark
TEST_HARNESS_DIR = tests/harness
TEST_BIN_DIR = tests/bin

# Create test directories if they don't exist
TEST_DIRS = $(TEST_UNIT_DIR) $(TEST_INTEG_DIR) $(TEST_BENCH_DIR) $(TEST_HARNESS_DIR)

# Find all test files (each becomes its own executable)
TEST_UNIT_SRCS = $(wildcard $(TEST_UNIT_DIR)/*.cpp)
TEST_INTEG_SRCS = $(wildcard $(TEST_INTEG_DIR)/*.cpp)
TEST_BENCH_SRCS = $(wildcard $(TEST_BENCH_DIR)/*.cpp)

# Generate executable names (respect current CONFIG)
TEST_UNIT_BINS = $(patsubst $(TEST_UNIT_DIR)/%.cpp,$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/%.unit,$(TEST_UNIT_SRCS))
TEST_INTEG_BINS = $(patsubst $(TEST_INTEG_DIR)/%.cpp,$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/%.integ,$(TEST_INTEG_SRCS))
TEST_BENCH_BINS = $(patsubst $(TEST_BENCH_DIR)/%.cpp,$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/%.bench,$(TEST_BENCH_SRCS))

# Common test flags (respect current CONFIG for consistency)
TEST_CXXFLAGS = $(CXXFLAGS)
TEST_LDFLAGS = $(LDFLAGS) -lnuma
TEST_INCLUDES = -I$(TEST_HARNESS_DIR) -I$(SRC_DIR) -Iinclude 

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
	@mkdir -p $(TEST_BIN_DIR)/$(BUILD_SUFFIX)
	@echo "Building with CONFIG=$(CONFIG)"
	@echo "Build directory: $(BUILD_DIR)"
	@echo "Binary directory: $(BIN_DIR)"
	@echo "Test binary directory: $(TEST_BIN_DIR)/$(BUILD_SUFFIX)"

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
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(SHARED_HEADERS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================================================
# Test Build Rules
# ============================================================================

# Build all unit tests (each as separate binary)
.PHONY: unit-tests
unit-tests: directories $(TEST_UNIT_BINS)
	@echo "✓ Built $(words $(TEST_UNIT_BINS)) unit test(s)"

# Build all integration tests
.PHONY: integration-tests
integration-tests: directories $(TEST_INTEG_BINS)
	@echo "✓ Built $(words $(TEST_INTEG_BINS)) integration test(s)"

# Build all test benchmarks
.PHONY: test-benchmarks
test-benchmarks: directories $(TEST_BENCH_BINS)
	@echo "✓ Built $(words $(TEST_BENCH_BINS)) test benchmark(s)"

# Build all tests (unit + integration)
.PHONY: tests
tests: unit-tests integration-tests
	@echo "✓ All tests built"

# ============================================================================
# EXPLICIT TEST RULES (Add this to your Makefile)
# ============================================================================

# First, find out what test files actually exist
TEST_FILES := $(shell ls tests/unit/*.cpp 2>/dev/null | xargs -n1 basename)
TEST_BINS := $(addprefix tests/bin/release/, $(TEST_FILES:.cpp=.unit))

# Build all unit tests
unit-tests: directories $(TEST_BINS)

# Generic rule for ANY unit test
tests/bin/release/%.unit: tests/unit/%.cpp
	@mkdir -p tests/bin/release
	$(CXX) $(TEST_CXXFLAGS) $(TEST_INCLUDES) $< -o $@ -lnuma
	@echo "✓ Built: $$(basename $@)"

# Run all unit tests (with proper exit code aggregation)
.PHONY: run-unit-tests
run-unit-tests: unit-tests
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@for test in $(TEST_UNIT_BINS); do \
		echo "Running: $$test"; \
		echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"; \
		$$test; \
	done

# Run all integration tests
.PHONY: run-integration-tests
run-integration-tests: integration-tests
	@failed=0; \
	passed=0; \
	total=0; \
	for test in $(TEST_INTEG_BINS); do \
		total=$$((total + 1)); \
		echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"; \
		echo "Running: $$test"; \
		echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"; \
		if $$test; then \
			passed=$$((passed + 1)); \
			echo "✓ PASSED"; \
		else \
			failed=$$((failed + 1)); \
			echo "✗ FAILED"; \
		fi; \
		echo ""; \
	done; \
	echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"; \
	echo "Integration Test Summary: $$passed passed, $$failed failed, $$total total"; \
	echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"; \
	exit $$failed

# Run all tests (unit + integration)
.PHONY: run-tests
run-tests: run-unit-tests run-integration-tests
	@echo "All tests completed"

# Run specific test (e.g., make run-test TEST=test_queue)
.PHONY: run-test
run-test: directories
	@if [ -z "$(TEST)" ]; then \
		echo "Error: Specify TEST=test_name (without .cpp extension)"; \
		echo "Example: make run-test TEST=test_queue"; \
		exit 1; \
	fi
	@if [ -f "$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/$(TEST).unit" ]; then \
		$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/$(TEST).unit; \
	elif [ -f "$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/$(TEST).integ" ]; then \
		$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/$(TEST).integ; \
	else \
		echo "Error: Test '$(TEST)' not found"; \
		exit 1; \
	fi

# Build rule for unit test binaries
$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/%.unit: $(TEST_UNIT_DIR)/%.cpp $(TEST_HARNESS_DIR)/test_runner.hpp
	@mkdir -p $(TEST_BIN_DIR)/$(BUILD_SUFFIX)
	$(CXX) $(TEST_CXXFLAGS) $(TEST_INCLUDES) $< -o $@
	@echo "✓ Built unit test: $@"

# Build rule for integration test binaries
$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/%.integ: $(TEST_INTEG_DIR)/%.cpp $(TEST_HARNESS_DIR)/test_runner.hpp
	@mkdir -p $(TEST_BIN_DIR)/$(BUILD_SUFFIX)
	$(CXX) $(TEST_CXXFLAGS) $(TEST_INCLUDES) $< -o $@
	@echo "✓ Built integration test: $@"

# Build rule for test benchmark binaries
$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/%.bench: $(TEST_BENCH_DIR)/%.cpp $(TEST_HARNESS_DIR)/test_runner.hpp
	@mkdir -p $(TEST_BIN_DIR)/$(BUILD_SUFFIX)
	$(CXX) $(TEST_CXXFLAGS) $(TEST_INCLUDES) $< -o $@
	@echo "✓ Built test benchmark: $@"

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

debug-tests:
	@$(MAKE) config=debug tests

# ============================================================================
# Release-Specific Targets (explicit)
# ============================================================================

release:
	@$(MAKE) config=release all

release-benchmark:
	@$(MAKE) config=release benchmark

release-run: release-benchmark
	@$(MAKE) config=release run_benchmark

release-tests:
	@$(MAKE) config=release tests

# ============================================================================
# Benchmark-Specific Config (optimized + symbols for perf)
# ============================================================================

benchmark-config:
	@$(MAKE) config=benchmark benchmark

benchmark-run: benchmark-config
	@$(MAKE) config=benchmark run_benchmark

benchmark-tests:
	@$(MAKE) config=benchmark tests

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

info:
	@echo "=== Build Configuration ==="
	@echo "Current CONFIG: $(CONFIG)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "Build dir: $(BUILD_DIR)"
	@echo "Binary dir: $(BIN_DIR)"
	@echo "Test dir: $(TEST_BIN_DIR)/$(BUILD_SUFFIX)"
	@echo ""
	@echo "Available targets:"
	@echo "  make                 - Release build (default)"
	@echo "  make debug           - Debug build (-O0 -g3)"
	@echo "  make release         - Release build (-O3)"
	@echo "  make benchmark-config - Benchmark build (-O3 with symbols)"
	@echo "  make tests           - Build all tests (unit + integration)"
	@echo "  make run-tests       - Build and run all tests"
	@echo "  make run-test TEST=name - Run specific test"
	@echo "  make unit-tests      - Build unit tests only"
	@echo "  make integration-tests - Build integration tests only"
	@echo "  make debug-benchmark - Debug benchmark"
	@echo "  make debug-run       - Run debug benchmark"
	@echo "  make run_benchmark   - Run benchmark (current config)"
	@echo "  make clean           - Clean everything"

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
	rm -rf $(TEST_BIN_DIR)
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

clean-tests:
	@echo "Cleaning test binaries..."
	rm -rf $(TEST_BIN_DIR)
	@echo "✓ Cleaned test binaries"

clean-all: clean

# ============================================================================
# Phony declarations
# ============================================================================

.PHONY: all directories library program benchmark
.PHONY: analyze_benchmark_performance check_benchmark_latency find_benchmark_error run_benchmark
.PHONY: analyze_performance check_latency find_error
.PHONY: run machine clean clean-release clean-debug clean-benchmark clean-all clean-tests
.PHONY: debug debug-benchmark debug-run debug-analyze debug-tests
.PHONY: release release-benchmark release-run release-tests
.PHONY: benchmark-config benchmark-run benchmark-tests
.PHONY: benchmark-link info
.PHONY: unit-tests integration-tests test-benchmarks tests
.PHONY: run-unit-tests run-integration-tests run-tests run-test
