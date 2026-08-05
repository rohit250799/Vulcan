// feed/include/vulcan/feed/Parser.h
#pragma once

#include "vulcan/core/Errorcode.h"
#include "vulcan/core/Result.h"
#include <cstddef>
#include <cstdint>

namespace vulcan::feed {

// A raw packet as received from the NIC (simplified).
struct Packet {
  const char *data;
  std::size_t length;
  uint64_t timestamp_ns; // arrival time from the capture card
};

// An order structure that the rest of the system understands.
struct Order {
  uint64_t order_id;
  uint64_t symbol_id;
  double price;
  uint32_t quantity;
  bool is_bid; // true = buy, false = sell
};

// Parse a single exchange packet into an Order.
// Returns an error if the packet is corrupt, unrecognised, or otherwise
// invalid.
[[nodiscard]] vulcan::core::Result<Order, vulcan::core::ErrorCode>
parse(const Packet &pkt) noexcept;

} // namespace vulcan::feed
