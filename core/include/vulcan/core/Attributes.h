#pragma once

#if defined(__GNUC__) || defined(__clang__)
#define VULCAN_LIKELY(x) __builtin_expect(!!(x), 1)
#define VULCAN_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define VULCAN_COLD __attribute__((cold))
#define VULCAN_NOINLINE __attribute__((noinline))
#else
#define VULCAN_LIKELY(x) (x)
#define VULCAN_UNLIKELY(x) (x)
#define VULCAN_COLD
#define VULCAN_NOINLINE
#endif