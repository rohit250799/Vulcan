// Vulcan/core/ErrorCode.h

#pragma once

#include <cstdint>

namespace vulcan::core {

// Single project-wide error code for all anticipated, recoverable failures.
// Each value must fit in 8 bits; add new codes sparingly.
// Values below 8 are reserved for common system-level errors.
enum class ErrorCode : uint8_t {
  // ---- Generic system errors (0–7) ----
  None = 0, // success (used only where Result isn't)
  Timeout = 1,
  ConnectionLost = 2,
  ResourceExhausted = 3, // e.g., ring buffer full, memory pool empty
  InvalidArgument = 4,

  // ---- Market data errors (8–11) ----
  CorruptPacket = 8,
  GapDetected = 9, // sequence number gap in a sequenced feed
  UnsupportedMsgType = 10,

  // ---- Order management errors (12–15) ----
  InvalidOrder = 12,
  RiskLimitBreached = 13,
  ExchangeReject = 14,

  // Reserve 15–31 for future needs; max 31 to fit in 5 bits if needed elsewhere
  Count = 32 // sentinel, not an error
};

// Debug helper – only for use in logging/telemetry, NEVER on the hot path.
// Defined out-of-line in Fatal.cpp to keep <cstdio> out of the header.
[[nodiscard]] const char *to_string(ErrorCode code) noexcept;

} // namespace vulcan::core