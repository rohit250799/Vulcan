add_executable(vulcan_benchmark benchmark.cpp)

target_link_libraries(vulcan_benchmark PRIVATE
  vulcan_core
  vulcan_feed
  vulcan_headers
  Threads::Threads
  ${NUMA_LIBRARY}
)

if(VULCAN_STRIP_RELEASE)
  add_custom_command(TARGET vulcan_benchmark POST_BUILD
    COMMAND "$<$<CONFIG:Release>:${CMAKE_STRIP}>" --strip-all "$<TARGET_FILE:vulcan_benchmark>"
    VERBATIM
  )
endif()

# perf/valgrind runs stay as plain shell — CMake has no business owning
# perf-stat invocations. Keep build_networking_state.sh /
# environment_hardening_script.sh as-is and call them before `perf`/`ctest` runs.
add_custom_target(run_benchmark
  COMMAND $<TARGET_FILE:vulcan_benchmark>
  DEPENDS vulcan_benchmark
  USES_TERMINAL
)
