#include "ran/cqi_capacity.hpp"

#include <cstdint>
#include <stdexcept>

namespace ran {

std::uint64_t bytesPerResourceBlock(int cqi) {
  if (cqi < 1 || cqi > 15) {
    throw std::invalid_argument("CQI must be between 1 and 15");
  }

  return static_cast<std::uint64_t>(cqi) * 100;
}

}  // namespace ran
