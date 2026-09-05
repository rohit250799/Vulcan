# NOTE: the old Makefile also built libhft.a from src/*.cpp (== main.cpp) and
# linked it alongside main.o into the same binary — that archive only ever
# contained a duplicate of main.o. Not reproducing it here; main.cpp goes
# straight into the executable.

add_executable(vulcan main.cpp)

target_link_libraries(vulcan PRIVATE
  vulcan_core
  vulcan_feed
  vulcan_headers
  Threads::Threads
  ${NUMA_LIBRARY}
)

if(VULCAN_STRIP_RELEASE)
  add_custom_command(TARGET vulcan POST_BUILD
    COMMAND "$<$<CONFIG:Release>:${CMAKE_STRIP}>" --strip-all "$<TARGET_FILE:vulcan>"
    COMMENT "Stripping release binary"
    VERBATIM
  )
endif()

# raw_socket_udp_listener needs CAP_NET_RAW — matches your old `run: sudo $(TARGET)`.
# Consider setcap instead of sudo long-term: 
#   sudo setcap cap_net_raw+ep $<TARGET_FILE:vulcan>
