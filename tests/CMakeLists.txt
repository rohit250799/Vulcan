# Keeps your existing tests/harness/test_runner.hpp — just wires each test
# file into CTest instead of a hand-rolled pass/fail loop in the Makefile.
#
# CAVEAT: file(GLOB ...) does NOT pick up new files without re-running cmake
# (it's evaluated at configure time, not build time). Fine while the suite is
# small; if that bites you, either switch to CONFIGURE_DEPENDS (re-globs on
# every build, minor cost) or list test files explicitly.

file(GLOB VULCAN_UNIT_TEST_SOURCES CONFIGURE_DEPENDS unit/*.cpp)
foreach(test_src ${VULCAN_UNIT_TEST_SOURCES})
  get_filename_component(test_name ${test_src} NAME_WE)
  set(target_name ${test_name}_unit)

  add_executable(${target_name} ${test_src})
  target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/harness)
  target_link_libraries(${target_name} PRIVATE vulcan_core vulcan_headers vulcan_warnings)

  add_test(NAME ${target_name} COMMAND ${target_name})
endforeach()

file(GLOB VULCAN_INTEG_TEST_SOURCES CONFIGURE_DEPENDS integration/*.cpp)
foreach(test_src ${VULCAN_INTEG_TEST_SOURCES})
  get_filename_component(test_name ${test_src} NAME_WE)
  set(target_name ${test_name}_integ)

  add_executable(${target_name} ${test_src})
  target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/harness)
  target_link_libraries(${target_name} PRIVATE vulcan_core vulcan_feed vulcan_headers vulcan_warnings ${NUMA_LIBRARY})

  add_test(NAME ${target_name} COMMAND ${target_name})
endforeach()

# `ctest --test-dir build/release` replaces `make run-unit-tests` /
# `make run-integration-tests`. `ctest -R name_unit` replaces `make run-test TEST=name`.
