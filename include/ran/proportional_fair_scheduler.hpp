#pragma once

#include "ran/i_scheduler.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace ran {

class ProportionalFairScheduler final : public IScheduler {
 public:
  explicit ProportionalFairScheduler(double epsilon = 1.0);

  std::vector<ResourceAllocation> schedule(
      std::span<const UeSchedulingView> ues,
      std::uint32_t availableResourceBlocks) override;

 private:
  double epsilon_;
};

}  // namespace ran
