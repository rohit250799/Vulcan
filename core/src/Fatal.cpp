#include "vulcan/core/Fatal.h"
#include "vulcan/core/Attributes.h" // VULCAN_COLD, VULCAN_NOINLINE
#include "vulcan/core/Errorcode.h"
#include <cstdio>   // snprintf (only in cold code)
#include <cstdlib>  // abort
#include <cstring>  // strerror_r
#include <unistd.h> // write

namespace vulcan::core {

// core/src/Fatal.cpp (add inside namespace vulcan::core)
const char *to_string(ErrorCode code) noexcept {
  switch (code) {
    using enum ErrorCode;
  case None:
    return "None";
  case Timeout:
    return "Timeout";
  case ConnectionLost:
    return "ConnectionLost";
  case ResourceExhausted:
    return "ResourceExhausted";
  case CorruptPacket:
    return "CorruptPacket";
  case InvalidArgument:
    return "InvalidArgument";
  default:
    return "Unknown";
  }
}

namespace {
// Convert errno to a string in a thread‑safe way.
void errno_string(int err, char *buf, size_t bufsize) noexcept {
  // XSI‑compliant version returns 0; GNU returns char*. We handle both.
  if (strerror_r(err, buf, bufsize) != 0) {
    snprintf(buf, bufsize, "Unknown errno %d", err);
  }
}
} // namespace

VULCAN_COLD VULCAN_NOINLINE void fatal(const char *msg) noexcept {
  constexpr int fd = STDERR_FILENO;
  auto len = __builtin_strlen(msg);
  [[maybe_unused]] auto n = write(fd, msg, len);
  (void)n;
  std::abort();
}

VULCAN_COLD VULCAN_NOINLINE void fatal(ErrorCode code, int sys_errno) noexcept {
  // Convert the error code to string (defined earlier)
  const char *code_str = to_string(code);

  // Convert errno to a string
  char err_buf[128];
  errno_string(sys_errno, err_buf, sizeof(err_buf));

  // Format a single message (stack buffer, no allocation)
  char msg[256];
  int len = snprintf(msg, sizeof(msg), "FATAL %s: %s\n", code_str, err_buf);

  if (len > 0) {
    [[maybe_unused]] auto n =
        write(STDERR_FILENO, msg, static_cast<size_t>(len));
    (void)n;
  }
  std::abort();
}

} // namespace vulcan::core