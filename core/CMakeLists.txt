# Builds libvulcan_core.a from core/src, exposes core/include/vulcan/core as PUBLIC
# so anything linking vulcan_core gets the headers automatically.

add_library(vulcan_core STATIC
  src/Fatal.cpp
  # Attributes.h / Errorcode.h / Result.h are currently header-only — add their
  # .cpp here if/when they grow implementation files.
)

target_include_directories(vulcan_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vulcan_core PUBLIC
  vulcan_warnings
  vulcan_arch_flags
  Threads::Threads
)
