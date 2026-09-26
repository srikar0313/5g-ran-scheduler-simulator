#pragma once

#include "ran/scheduling.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace ran {

class IScheduler {
 public:
  virtual ~IScheduler() = default;

  virtual std::vector<ResourceAllocation> schedule(
      std::span<const UeSchedulingView> ues,
      std::uint32_t availableResourceBlocks) = 0;
};

}  // namespace ran
