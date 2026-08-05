// feed/include/vulcan/feed/ErrorHandling.h
#pragma once

#include "vulcan/core/Errorcode.h"

namespace vulcan::feed {

// Increment an atomic counter for the given error code.
// This is a cold function – never inline it.
void record_parse_error(vulcan::core::ErrorCode code) noexcept;

// Optionally, push diagnostic info into a lock‑free ring buffer for a
// background logger. (We show only the declaration; real implementations might
// push a small struct.)
void push_diagnostic(vulcan::core::ErrorCode code, uint64_t order_id) noexcept;

} // namespace vulcan::feed