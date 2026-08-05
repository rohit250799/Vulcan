// core/include/vulcan/core/Fatal.h

#pragma once
#include "vulcan/core/Errorcode.h"

namespace vulcan::core {

// Simple fatal – just a static message.
[[noreturn]] void fatal(const char *msg) noexcept;

// Fatal with an ErrorCode and a raw errno (e.g., from bind()).
// The final message is formatted in Fatal.cpp – only called on termination.
[[noreturn]] void fatal(ErrorCode code, int sys_errno) noexcept;

} // namespace vulcan::core