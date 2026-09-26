#pragma once

#include "ran/i_scheduler.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ran {

class RoundRobinScheduler final : public IScheduler {
 public:
  std::vector<ResourceAllocation> schedule(
      std::span<const UeSchedulingView> ues,
      std::uint32_t availableResourceBlocks) override;

 private:
  // UE input order is expected to remain stable between scheduling calls.
  std::size_t nextUeIndex_{0};
};

}  // namespace ran
