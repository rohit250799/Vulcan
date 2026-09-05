cmake_minimum_required(VERSION 3.25)
project(Vulcan LANGUAGES CXX)

# ============================================================================
# Global settings
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # needed for clangd / clang-tidy

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# A 4th config, mirroring your old CONFIG=benchmark: release-level
# optimization but with symbols kept and -march=native for local profiling.
# Single-config generators (Make/Ninja) are fine defining arbitrary
# CMAKE_CXX_FLAGS_<CONFIG> values like this.
set(CMAKE_CXX_FLAGS_BENCHMARK "-O3 -g -DNDEBUG -fno-omit-frame-pointer" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_BENCHMARK "" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG
  "-O0 -g3 -gdwarf-4 -ggdb -fno-omit-frame-pointer -fno-inline -fno-eliminate-unused-debug-types -fno-optimize-sibling-calls"
  CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG" CACHE STRING "" FORCE)

# ============================================================================
# Options
# ============================================================================
option(VULCAN_NATIVE_ARCH   "Use -march=native instead of -march=znver3" OFF)
option(VULCAN_ENABLE_LTO    "Enable LTO in Release builds" ON)
option(VULCAN_BUILD_TESTS   "Build unit/integration tests" ON)
option(VULCAN_BUILD_BENCHMARKS "Build the benchmark binary" ON)
option(VULCAN_STRIP_RELEASE "Strip symbols from Release binaries" ON)

# ============================================================================
# Shared interface targets (warnings / arch flags) — link these into
# anything that compiles, instead of repeating flags per target.
# ============================================================================
add_library(vulcan_warnings INTERFACE)
target_compile_options(vulcan_warnings INTERFACE -Wall -Wextra -Wpedantic)

add_library(vulcan_arch_flags INTERFACE)
if(VULCAN_NATIVE_ARCH)
  target_compile_options(vulcan_arch_flags INTERFACE -march=native -mcx16)
else()
  target_compile_options(vulcan_arch_flags INTERFACE -march=znver3 -mcx16)
endif()

# ============================================================================
# Dependencies
# ============================================================================
find_package(Threads REQUIRED)

find_library(NUMA_LIBRARY numa)
if(NOT NUMA_LIBRARY)
  message(FATAL_ERROR "libnuma not found — install numactl-devel (Fedora) / libnuma-dev (Debian)")
endif()

add_library(vulcan_headers INTERFACE)
target_include_directories(vulcan_headers INTERFACE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(vulcan_headers INTERFACE ${NUMA_LIBRARY})

if(VULCAN_ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
  if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
  else()
    message(WARNING "LTO requested but not supported: ${ipo_error}")
  endif()
endif()

# ============================================================================
# Subprojects — mirrors your directory layout 1:1
# ============================================================================
add_subdirectory(core)
add_subdirectory(feed)
add_subdirectory(src)

if(VULCAN_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(VULCAN_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

# ============================================================================
# Convenience targets: format
# ============================================================================
find_program(CLANG_FORMAT_EXE clang-format)
if(CLANG_FORMAT_EXE)
  file(GLOB_RECURSE VULCAN_ALL_SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/core/src/*.cpp ${CMAKE_SOURCE_DIR}/core/include/*.h
    ${CMAKE_SOURCE_DIR}/feed/src/*.cpp ${CMAKE_SOURCE_DIR}/feed/include/*.h ${CMAKE_SOURCE_DIR}/feed/include/*.hpp
    ${CMAKE_SOURCE_DIR}/include/vulcan/*.hpp
    ${CMAKE_SOURCE_DIR}/tests/unit/*.cpp ${CMAKE_SOURCE_DIR}/tests/integration/*.cpp
  )
  add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXE} -i ${VULCAN_ALL_SOURCES}
    COMMENT "Running clang-format over the tree"
  )
endif()
