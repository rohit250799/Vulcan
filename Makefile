CXX = g++
CXXFLAGS = -Iinclude -Wall -Wextra -std=c++17

BUILD_DIR = build
BIN_DIR = bin
LIB_DIR = lib

TARGET = $(BIN_DIR)/vulcan
LIBRARY = $(LIB_DIR)/libhft.a

SRC = $(wildcard src/*.cpp)
OBJ = $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(SRC))

MAIN_OBJ = $(BUILD_DIR)/main.o

TEST_SRC = $(wildcard tests/*.cpp)
TEST_OBJ = $(patsubst tests/%.cpp,$(BUILD_DIR)/%.test.o,$(TEST_SRC))
TEST_BIN = $(BIN_DIR)/tests_runner

# Default
all: directories library program tests


# Create directories
directories:
	mkdir -p $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR)


# Build static library
library: $(LIBRARY)

$(LIBRARY): $(OBJ)
	ar rcs $(LIBRARY) $(OBJ)

# Ensure -mcx16 is in your compilation flags
CXXFLAGS += -O3 -mcx16 -march=znver3 -lhft -std=c++20
LDFLAGS  += -mcx16 -march=znver3

# Build main program
program: $(TARGET)

$(TARGET): $(MAIN_OBJ) $(LIBRARY)
	$(CXX) $(MAIN_OBJ) -L$(LIB_DIR) -march=znver3 -mcx16 -lhft -o $(TARGET)


# Compile main
$(BUILD_DIR)/main.o: main.cpp
	$(CXX) $(CXXFLAGS) -g -c main.cpp -o $@


# Compile src files
$(BUILD_DIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@


# Build tests
tests: $(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(LIBRARY)
	$(CXX) $(CXXFLAGS) $^ -L$(LIB_DIR) -lhft -o $@

# Run main
run:
	./$(TARGET)


# Run tests
run_tests: $(TEST_BIN)
	./$(TEST_BIN)


# Clean
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR)

#performance analysis
analyze_performance:
	perf stat -e cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses ./bin/vulcan

