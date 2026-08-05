# ============================================================================
# Add auto dependency generation in Makefile
DEPFLAGS = -MMD -MP
CXXFLAGS += $(DEPFLAGS)
# After building objects, include .d files
-include $(SHARED_OBJECTS:.o=.d) $(MAIN_OBJ:.o=.d)

# ============================================================================
# Production Makefile for Vulcan Core (Quant Firm Grade)
# With Separate Debug and Release Configurations
# ============================================================================

CXX = g++

# ============================================================================
# Configuration Selection
# ============================================================================
CONFIG ?= release

# ============================================================================
# Compiler Flags per Configuration
# ============================================================================

BASE_CXXFLAGS = -std=c++20 -Iinclude -Wall -Wextra -Wpedantic -pthread
BASE_LDFLAGS = -lpthread -lnuma

ifeq ($(CONFIG),release)
    CXXFLAGS = $(BASE_CXXFLAGS) -O3 -mcx16 -march=znver3 -DNDEBUG -flto
    LDFLAGS = $(BASE_LDFLAGS) -flto
    BUILD_SUFFIX = release
    STRIP_SYMBOLS = yes
endif

ifeq ($(CONFIG),debug)
    CXXFLAGS = $(BASE_CXXFLAGS) -O0 -g3 -gdwarf-4 -ggdb -DDEBUG -fno-omit-frame-pointer
    CXXFLAGS += -fno-inline -fno-eliminate-unused-debug-types -fno-optimize-sibling-calls
    LDFLAGS = $(BASE_LDFLAGS)
    BUILD_SUFFIX = debug
    STRIP_SYMBOLS = no
endif

ifeq ($(CONFIG),benchmark)
    CXXFLAGS = $(BASE_CXXFLAGS) -O3 -mcx16 -march=native -DNDEBUG -g -fno-omit-frame-pointer
    LDFLAGS = $(BASE_LDFLAGS)
    BUILD_SUFFIX = benchmark
    STRIP_SYMBOLS = no
endif

# ============================================================================
# Directories
# ============================================================================
ROOT_DIR := $(shell pwd)
SRC_DIR = $(ROOT_DIR)/src

CORE_SRC_DIR = $(ROOT_DIR)/core/src
CORE_INC_DIR = $(ROOT_DIR)/core/include

FEED_SRC_DIR  = $(ROOT_DIR)/feed/src
FEED_INC_DIR  = $(ROOT_DIR)/feed/include

INC = -I$(CORE_INC_DIR) -I$(FEED_INC_DIR) -Iinclude

BUILD_DIR = $(ROOT_DIR)/build/$(BUILD_SUFFIX)
BIN_DIR = $(ROOT_DIR)/bin/$(BUILD_SUFFIX)
LIB_DIR = $(ROOT_DIR)/lib/$(BUILD_SUFFIX)

BENCHMARK_DIR = $(ROOT_DIR)/benchmarks
BENCHMARK_BUILD_DIR = $(BENCHMARK_DIR)/build/$(BUILD_SUFFIX)
BENCHMARK_BIN_DIR = $(BENCHMARK_DIR)/bin/$(BUILD_SUFFIX)
BENCHMARK_RESULTS_DIR = $(BENCHMARK_DIR)/results

# ============================================================================
# Sources and Objects
# ============================================================================

CORE_SOURCES = $(wildcard $(CORE_SRC_DIR)/*.cpp)
CORE_OBJECTS = $(patsubst $(CORE_SRC_DIR)/%.cpp,$(BUILD_DIR)/core/%.o,$(CORE_SOURCES))
CORE_LIB     = $(LIB_DIR)/libvulcan_core.a

FEED_SOURCES = $(wildcard $(FEED_SRC_DIR)/*.cpp)
FEED_OBJECTS = $(patsubst $(FEED_SRC_DIR)/%.cpp,$(BUILD_DIR)/feed/%.o,$(FEED_SOURCES))
FEED_LIB     = $(LIB_DIR)/libvulcan_feed.a

SHARED_SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
SHARED_HEADERS = $(wildcard $(SRC_DIR)/*.h) $(wildcard $(SRC_DIR)/*.hpp)
SHARED_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SHARED_SOURCES))

MAIN_SRC = $(SRC_DIR)/main.cpp
MAIN_OBJ = $(BUILD_DIR)/main.o

LIBRARY = $(LIB_DIR)/libhft.a
TARGET  = $(BIN_DIR)/vulcan

# Benchmark sources (exclude main.cpp to avoid duplicate main)
BENCHMARK_SHARED_SOURCES = $(filter-out $(MAIN_SRC), $(SHARED_SOURCES))
BENCHMARK_SRC = $(BENCHMARK_DIR)/benchmark.cpp
BENCHMARK_CORE_OBJECTS = $(patsubst $(CORE_SRC_DIR)/%.cpp,$(BENCHMARK_BUILD_DIR)/core/%.o,$(CORE_SOURCES))
BENCHMARK_OBJECTS = $(BENCHMARK_BUILD_DIR)/benchmark.o \
                    $(patsubst $(SRC_DIR)/%.cpp,$(BENCHMARK_BUILD_DIR)/%.o,$(BENCHMARK_SHARED_SOURCES)) \
                    $(BENCHMARK_CORE_OBJECTS)
BENCHMARK_TARGET = $(BENCHMARK_BIN_DIR)/benchmark

# ============================================================================
# Test Harness
# ============================================================================
TEST_UNIT_DIR = tests/unit
TEST_INTEG_DIR = tests/integration
TEST_BENCH_DIR = tests/benchmark
TEST_HARNESS_DIR = tests/harness
TEST_BIN_DIR = tests/bin

TEST_UNIT_SRCS = $(wildcard $(TEST_UNIT_DIR)/*.cpp)
TEST_INTEG_SRCS = $(wildcard $(TEST_INTEG_DIR)/*.cpp)
TEST_BENCH_SRCS = $(wildcard $(TEST_BENCH_DIR)/*.cpp)

TEST_UNIT_BINS = $(patsubst $(TEST_UNIT_DIR)/%.cpp,$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/%.unit,$(TEST_UNIT_SRCS))
TEST_INTEG_BINS = $(patsubst $(TEST_INTEG_DIR)/%.cpp,$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/%.integ,$(TEST_INTEG_SRCS))
TEST_BENCH_BINS = $(patsubst $(TEST_BENCH_DIR)/%.cpp,$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/%.bench,$(TEST_BENCH_SRCS))

TEST_CXXFLAGS = $(CXXFLAGS)
TEST_LDFLAGS = $(LDFLAGS) -lnuma
TEST_INCLUDES = -I$(TEST_HARNESS_DIR) -I$(SRC_DIR) -Iinclude -I$(CORE_INC_DIR)

# ============================================================================
# Default Goal
# ============================================================================
.DEFAULT_GOAL := all

# ============================================================================
# Production Build
# ============================================================================
all: directories core-library feed-library library program

directories:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR) $(BENCHMARK_RESULTS_DIR)
	@mkdir -p $(BUILD_DIR)/core $(BUILD_DIR)/feed $(BENCHMARK_BUILD_DIR)/core
	@mkdir -p $(TEST_BIN_DIR)/$(BUILD_SUFFIX)
	@echo "Building with CONFIG=$(CONFIG)"

core-library: $(CORE_LIB)

$(CORE_LIB): $(CORE_OBJECTS)
	ar rcs $@ $^

$(BUILD_DIR)/core/%.o: $(CORE_SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

feed-library: $(FEED_LIB)

$(FEED_LIB): $(FEED_OBJECTS)
	ar rcs $@ $^

$(BUILD_DIR)/feed/%.o: $(FEED_SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

library: $(LIBRARY)

$(LIBRARY): $(SHARED_OBJECTS)
	ar rcs $@ $^

program: $(TARGET)

$(TARGET): $(MAIN_OBJ) $(LIBRARY) $(CORE_LIB) $(FEED_LIB)
	$(CXX) $(CXXFLAGS) -o $@ $^ -L$(LIB_DIR) -lhft -lvulcan_core -lvulcan_feed $(LDFLAGS)
	@if [ "$(STRIP_SYMBOLS)" = "yes" ]; then \
		echo "Stripping symbols from $(TARGET)"; \
		strip --strip-all $(TARGET); \
	fi
	@echo "Production binary built: $(TARGET)"

$(BUILD_DIR)/main.o: $(MAIN_SRC)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(SHARED_HEADERS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

# ============================================================================
# Tests
# ============================================================================
unit-tests: directories core-library $(TEST_UNIT_BINS)
	@echo "✓ Built $(words $(TEST_UNIT_BINS)) unit test(s)"

integration-tests: directories core-library $(TEST_INTEG_BINS)
	@echo "✓ Built $(words $(TEST_INTEG_BINS)) integration test(s)"

test-benchmarks: directories core-library $(TEST_BENCH_BINS)
	@echo "✓ Built $(words $(TEST_BENCH_BINS)) test benchmark(s)"

tests: unit-tests integration-tests
	@echo "✓ All tests built"

run-unit-tests: unit-tests
	@for test in $(TEST_UNIT_BINS); do \
		echo "Running: $$test"; \
		$$test; \
	done

run-integration-tests: integration-tests
	@failed=0; passed=0; total=0; \
	for test in $(TEST_INTEG_BINS); do \
		total=$$((total + 1)); \
		echo "Running: $$test"; \
		if $$test; then passed=$$((passed + 1)); echo "✓ PASSED"; \
		else failed=$$((failed + 1)); echo "✗ FAILED"; fi; \
	done; \
	echo "Summary: $$passed passed, $$failed failed, $$total total"; \
	exit $$failed

run-tests: run-unit-tests run-integration-tests
	@echo "All tests completed"

run-test: directories core-library
	@if [ -z "$(TEST)" ]; then echo "Specify TEST=name"; exit 1; fi
	@if [ -f "$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/$(TEST).unit" ]; then \
		$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/$(TEST).unit; \
	elif [ -f "$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/$(TEST).integ" ]; then \
		$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/$(TEST).integ; \
	else echo "Test '$(TEST)' not found"; exit 1; fi

$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/%.unit: $(TEST_UNIT_DIR)/%.cpp $(TEST_HARNESS_DIR)/test_runner.hpp $(CORE_LIB)
	@mkdir -p $(@D)
	$(CXX) $(TEST_CXXFLAGS) $(TEST_INCLUDES) $< $(CORE_LIB) -o $@ $(TEST_LDFLAGS)
	@echo "✓ Built unit test: $@"

$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/%.integ: $(TEST_INTEG_DIR)/%.cpp $(TEST_HARNESS_DIR)/test_runner.hpp $(CORE_LIB)
	@mkdir -p $(@D)
	$(CXX) $(TEST_CXXFLAGS) $(TEST_INCLUDES) $< $(CORE_LIB) -o $@ $(TEST_LDFLAGS)
	@echo "✓ Built integration test: $@"

$(TEST_BIN_DIR)/$(BUILD_SUFFIX)/%.bench: $(TEST_BENCH_DIR)/%.cpp $(TEST_HARNESS_DIR)/test_runner.hpp $(CORE_LIB)
	@mkdir -p $(@D)
	$(CXX) $(TEST_CXXFLAGS) $(TEST_INCLUDES) $< $(CORE_LIB) -o $@ $(TEST_LDFLAGS)
	@echo "✓ Built test benchmark: $@"

# ============================================================================
# Benchmark
# ============================================================================
benchmark: $(BENCHMARK_TARGET)

$(BENCHMARK_TARGET): $(BENCHMARK_OBJECTS)
	@mkdir -p $(BENCHMARK_BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@if [ "$(STRIP_SYMBOLS)" = "yes" ]; then strip --strip-all $@; fi
	@echo "Benchmark built: $@"

$(BENCHMARK_BUILD_DIR)/benchmark.o: $(BENCHMARK_SRC) $(SHARED_HEADERS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -Iinclude -I$(CORE_INC_DIR) -I$(FEED_INC_DIR) -c $< -o $@

$(BENCHMARK_BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(SHARED_HEADERS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -Iinclude -I$(CORE_INC_DIR) -I$(FEED_INC_DIR) -c $< -o $@

$(BENCHMARK_BUILD_DIR)/core/%.o: $(CORE_SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -Iinclude -I$(CORE_INC_DIR) -I$(FEED_INC_DIR) -c $< -o $@

# ============================================================================
# Convenience targets
# ============================================================================
benchmark-link: benchmark
	@ln -sf $(BENCHMARK_TARGET) $(ROOT_DIR)/benchmark
	@echo "Symlink created: ./benchmark -> $(BENCHMARK_TARGET)"

analyze_benchmark_performance: benchmark
	@mkdir -p $(BENCHMARK_RESULTS_DIR)
	sudo perf stat -x, -e cycles,instructions,branches,branch-misses,cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,dTLB-loads,dTLB-load-misses,stalled-cycles-frontend,stalled-cycles-backend,context-switches,cpu-migrations,page-faults -o $(BENCHMARK_RESULTS_DIR)/unified_stats_$(CONFIG).csv $(BENCHMARK_TARGET)
	@column -s, -t < $(BENCHMARK_RESULTS_DIR)/unified_stats_$(CONFIG).csv

check_benchmark_latency: benchmark
	sudo perf mem record -d $(BENCHMARK_TARGET) -o $(BENCHMARK_RESULTS_DIR)/latency_$(CONFIG).data
	sudo perf mem report --stdio --verbose > $(BENCHMARK_RESULTS_DIR)/latency_report_$(CONFIG).txt
	sudo perf c2c record -o $(BENCHMARK_RESULTS_DIR)/c2c_$(CONFIG).data $(BENCHMARK_TARGET)
	sudo perf c2c report --stdio > $(BENCHMARK_RESULTS_DIR)/c2c_report_$(CONFIG).txt

find_benchmark_error: benchmark-link
	gdb -ex "run" -ex "backtrace" -ex "quit" ./benchmark

run_benchmark: benchmark-link
	./benchmark

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

release:
	@$(MAKE) config=release all

release-benchmark:
	@$(MAKE) config=release benchmark

release-run: release-benchmark
	@$(MAKE) config=release run_benchmark

release-tests:
	@$(MAKE) config=release tests

benchmark-config:
	@$(MAKE) config=benchmark benchmark

benchmark-run: benchmark-config
	@$(MAKE) config=benchmark run_benchmark

benchmark-tests:
	@$(MAKE) config=benchmark tests

analyze_performance: program
	mkdir -p perf_results
	perf stat -e cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,dTLB-load-misses $(TARGET) > perf_results/stat_1_$(CONFIG).txt 2>&1
	perf stat -e cycles,instructions,cache-misses,branch-misses $(TARGET) > perf_results/stat_2_$(CONFIG).txt 2>&1
	sudo perf mem record -d $(TARGET) -o perf_results/mem_$(CONFIG).data
	sudo perf mem report -f --stdio --sort=sym --percent-limit=0 -i perf_results/mem_$(CONFIG).data > perf_results/mem_report_$(CONFIG).txt

check_latency: program
	sudo perf mem record -d $(TARGET)
	sudo perf mem report --stdio --verbose
	sudo perf c2c report $(TARGET)

find_error: program
	gdb -ex "run" -ex "backtrace" -ex "quit" $(TARGET)

run: program
	$(TARGET)

machine:
	objdump -D $(BUILD_DIR)/main.o

info:
	@echo "=== Build Configuration ==="
	@echo "CONFIG: $(CONFIG)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "Build dir: $(BUILD_DIR)"
	@echo "Binary dir: $(BIN_DIR)"
	@echo "Core lib: $(CORE_LIB)"
	@echo "Feed lib: $(FEED_LIB)"
	@echo "App lib: $(LIBRARY)"
	@echo "Test bin dir: $(TEST_BIN_DIR)/$(BUILD_SUFFIX)"

# ============================================================================
# Clean
# ============================================================================
clean:
	rm -rf $(ROOT_DIR)/build $(ROOT_DIR)/bin $(ROOT_DIR)/lib
	rm -rf $(BENCHMARK_BUILD_DIR) $(BENCHMARK_BIN_DIR)
	rm -f $(ROOT_DIR)/benchmark
	rm -rf $(TEST_BIN_DIR)
	@echo "Clean complete"

clean-release:
	rm -rf $(ROOT_DIR)/build/release $(ROOT_DIR)/bin/release $(ROOT_DIR)/lib/release
	rm -rf $(BENCHMARK_DIR)/build/release $(BENCHMARK_DIR)/bin/release

clean-debug:
	rm -rf $(ROOT_DIR)/build/debug $(ROOT_DIR)/bin/debug $(ROOT_DIR)/lib/debug
	rm -rf $(BENCHMARK_DIR)/build/debug $(BENCHMARK_DIR)/bin/debug

clean-benchmark:
	rm -rf $(BENCHMARK_RESULTS_DIR)

clean-tests:
	rm -rf $(TEST_BIN_DIR)

clean-all: clean

# ============================================================================
# Format
# ============================================================================
.PHONY: format
format:
	find $(SRC_DIR) include core/src core/include feed/src feed/include tests -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -exec clang-format -i {} +

# ============================================================================
# Valgrind
# ============================================================================
.PHONY: valgrind
valgrind: program
	valgrind --leak-check=full --show-leak-kinds=all $(TARGET)

.PHONY: valgrind-benchmark
valgrind-benchmark: benchmark
	valgrind --leak-check=full $(BENCHMARK_TARGET)

# ============================================================================
# Phony declarations
# ============================================================================
.PHONY: all directories core-library feed-library library program benchmark
.PHONY: unit-tests integration-tests test-benchmarks tests run-unit-tests run-integration-tests run-tests run-test
.PHONY: debug debug-benchmark debug-run debug-analyze debug-tests
.PHONY: release release-benchmark release-run release-tests
.PHONY: benchmark-config benchmark-run benchmark-tests benchmark-link
.PHONY: analyze_benchmark_performance check_benchmark_latency find_benchmark_error run_benchmark
.PHONY: analyze_performance check_latency find_error run machine info
.PHONY: clean clean-release clean-debug clean-benchmark clean-tests clean-all
.PHONY: format valgrind valgrind-benchmark
