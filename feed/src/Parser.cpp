// feed/src/Parser.cpp
#include "vulcan/feed/Parser.h"
#include "vulcan/core/Attributes.h" // VULCAN_UNLIKELY, VULCAN_LIKELY
#include "vulcan/core/Errorcode.h"
#include "vulcan/feed/ErrorHandling.h"
#include <cstdint>
#include <utility>

#include <algorithm> // std::min
#include <cstring>   // memcpy

namespace vulcan::feed {

using vulcan::core::ErrorCode;
using vulcan::core::Result;

// -------------------------------------------------------------------
// Internal helper functions (private to this translation unit)
// -------------------------------------------------------------------

namespace {

// Simulate extracting a field and checking bounds.
// In reality this would be a templated byte‑swapping function.
template <typename T>
Result<T, ErrorCode> extract_field(const Packet &pkt,
                                   std::size_t offset) noexcept {
  if (VULCAN_UNLIKELY(offset + sizeof(T) > pkt.length)) {
    return ErrorCode::CorruptPacket;
  }
  T value;
  std::memcpy(&value, pkt.data + offset, sizeof(T));
  return value; // implicit conversion to Result<T, ErrorCode> (success)
}

} // anonymous namespace

// -------------------------------------------------------------------
// Public parsing function
// -------------------------------------------------------------------

Result<Order, ErrorCode> parse(const Packet &pkt) noexcept {
  // Exchange wire format (example):
  // [0..7]  order_id (uint64_t, little endian)
  // [8..9]  symbol_id (uint16_t)
  // [10..17] price (double)
  // [18..21] quantity (uint32_t)
  // [22]    side (0 = bid, 1 = ask)
  // [23..]  reserved / checksum

  // 1. Extract order_id
  auto id_res = extract_field<uint64_t>(pkt, 0);
  if (VULCAN_UNLIKELY(!id_res.has_value())) {
    record_parse_error(std::move(id_res).error());
    return std::move(id_res).error();
  }
  uint64_t order_id = std::move(id_res).value();

  // 2. Extract symbol_id
  auto sym_res = extract_field<uint16_t>(pkt, 8);
  if (VULCAN_UNLIKELY(!sym_res.has_value())) {
    record_parse_error(std::move(sym_res).error());
    return std::move(sym_res).error();
  }
  uint64_t symbol_id = std::move(sym_res).value(); // widen to 64 bits

  // 3. Extract price
  auto price_res = extract_field<double>(pkt, 10);
  if (VULCAN_UNLIKELY(!price_res.has_value())) {
    record_parse_error(std::move(price_res).error());
    return std::move(price_res).error();
  }
  double price = std::move(price_res).value();

  // 4. Extract quantity
  auto qty_res = extract_field<uint32_t>(pkt, 18);
  if (VULCAN_UNLIKELY(!qty_res.has_value())) {
    record_parse_error(std::move(qty_res).error());
    return std::move(qty_res).error();
  }
  uint32_t quantity = std::move(qty_res).value();

  // 5. Extract side (0 = bid, 1 = ask)
  if (VULCAN_UNLIKELY(22 + 1 > pkt.length)) {
    record_parse_error(ErrorCode::CorruptPacket);
    return ErrorCode::CorruptPacket;
  }
  uint8_t side_byte = pkt.data[22];
  if (VULCAN_UNLIKELY(side_byte > 1)) {
    // Unrecognised side value – this is a programming bug assumption
    // because the exchange spec says it's always 0 or 1.
    // We treat it as a fatal corruption (but not abort – just drop the packet).
    record_parse_error(ErrorCode::UnsupportedMsgType);
    return ErrorCode::UnsupportedMsgType;
  }
  bool is_bid = (side_byte == 0);

  // Success – construct the order
  return Order{.order_id = order_id,
               .symbol_id = symbol_id,
               .price = price,
               .quantity = quantity,
               .is_bid = is_bid};
}

} // namespace vulcan::feed