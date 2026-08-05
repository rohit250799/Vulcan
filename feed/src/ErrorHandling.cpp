// feed/src/ErrorHandling.cpp
#include "vulcan/feed/ErrorHandling.h"
#include "vulcan/core/Attributes.h" // VULCAN_COLD, VULCAN_NOINLINE
#include <atomic>
#include <cstdint>

namespace vulcan::feed {

// Atomic counters – defined once, visible only inside this translation unit.
namespace {
std::atomic<uint64_t> g_corrupt_packets{0};
std::atomic<uint64_t> g_timeouts{0};
std::atomic<uint64_t> g_unsupported_msg{0};
std::atomic<uint64_t> g_other_errors{0};
} // namespace

VULCAN_COLD VULCAN_NOINLINE void
record_parse_error(vulcan::core::ErrorCode code) noexcept {
  switch (code) {
    using enum vulcan::core::ErrorCode;
  case CorruptPacket:
    g_corrupt_packets.fetch_add(1, std::memory_order_relaxed);
    break;
  case Timeout:
    g_timeouts.fetch_add(1, std::memory_order_relaxed);
    break;
  case UnsupportedMsgType:
    g_unsupported_msg.fetch_add(1, std::memory_order_relaxed);
    break;
  default:
    g_other_errors.fetch_add(1, std::memory_order_relaxed);
    break;
  }
}

VULCAN_COLD VULCAN_NOINLINE void push_diagnostic(vulcan::core::ErrorCode code,
                                                 uint64_t order_id) noexcept {
  // In a real system this would push a tiny struct into a lock‑free SPSC
  // queue that a background thread drains and writes to a log file.
  // No string formatting, no allocation, no syscalls here.
  // We leave it as a stub for illustration.
  (void)code;
  (void)order_id;
}

} // namespace vulcan::feed