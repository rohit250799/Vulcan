add_library(vulcan_feed STATIC
  src/ErrorHandling.cpp
  src/Parser.cpp
  src/raw_socket_udp_listener.cpp
)

target_include_directories(vulcan_feed PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# feed depends on core (Errorcode/Result types presumably feed into
# ErrorHandling.cpp) — adjust if that's not actually the dependency direction.
target_link_libraries(vulcan_feed PUBLIC
  vulcan_core
  vulcan_warnings
  vulcan_arch_flags
)
