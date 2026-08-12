// core/include/vulcan/core/Result.h
#pragma once

#include "Errorcode.h"
#include <cassert>
#include <new>
#include <utility>

namespace vulcan::core {

template <typename T, typename E = ErrorCode> class [[nodiscard]] Result {
public:
  // Success constructor
  Result(T &&value) noexcept : has_value_(true) {
    new (&val_) T(std::move(value));
  }

  // Error constructor
  Result(E &&error) noexcept : has_value_(false) {
    new (&err_) E(std::move(error));
  }

  ~Result() noexcept {
    if (has_value_)
      val_.~T();
    else
      err_.~E();
  }

  // Move constructor (fixed)
  Result(Result &&other) noexcept : has_value_(other.has_value_) {
    if (has_value_)
      new (&val_) T(std::move(other.val_));
    else
      new (&err_) E(std::move(other.err_));
  }

  // No copying
  Result(const Result &) = delete;
  Result &operator=(const Result &) = delete;

  [[nodiscard]] bool has_value() const noexcept { return has_value_; }

  // Get the value – only for rvalues (use std::move to call)
  [[nodiscard]] T &&value() && noexcept {
    assert(has_value_);
    return std::move(val_);
  }

  // Get the error – only for rvalues (use std::move to call)
  [[nodiscard]] E &&error() && noexcept {
    assert(!has_value_);
    return std::move(err_);
  }

private:
  union {
    T val_;
    E err_;
  };
  bool has_value_;
};

} // namespace vulcan::core