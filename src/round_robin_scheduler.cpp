#include "ran/round_robin_scheduler.hpp"

#include "ran/cqi_capacity.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ran {
namespace {

std::uint64_t requiredResourceBlocks(const UeSchedulingView& ue) {
  if (ue.queuedBytes == 0) {
    return 0;
  }

  const auto bytesPerRb = bytesPerResourceBlock(ue.cqi);
  const auto completeBlocks = ue.queuedBytes / bytesPerRb;
  const auto partialBlock = ue.queuedBytes % bytesPerRb == 0 ? 0U : 1U;
  return completeBlocks + partialBlock;
}

}  // namespace

std::vector<ResourceAllocation> RoundRobinScheduler::schedule(
    std::span<const UeSchedulingView> ues, std::uint32_t availableResourceBlocks) {
  if (ues.empty() || availableResourceBlocks == 0) {
    return {};
  }

  std::vector<std::uint64_t> remainingDemand;
  remainingDemand.reserve(ues.size());
  std::size_t activeUes = 0;
  for (const auto& ue : ues) {
    const auto demand = requiredResourceBlocks(ue);
    remainingDemand.push_back(demand);
    if (demand > 0) {
      ++activeUes;
    }
  }

  if (activeUes == 0) {
    return {};
  }

  std::vector<std::uint32_t> allocated(ues.size(), 0);
  auto currentIndex = nextUeIndex_ % ues.size();
  auto remainingResourceBlocks = availableResourceBlocks;

  while (remainingResourceBlocks > 0 && activeUes > 0) {
    if (remainingDemand[currentIndex] > 0) {
      ++allocated[currentIndex];
      --remainingDemand[currentIndex];
      --remainingResourceBlocks;
      if (remainingDemand[currentIndex] == 0) {
        --activeUes;
      }
    }
    currentIndex = (currentIndex + 1) % ues.size();
  }

  nextUeIndex_ = currentIndex;

  std::vector<ResourceAllocation> allocations;
  allocations.reserve(ues.size());
  for (std::size_t index = 0; index < ues.size(); ++index) {
    if (allocated[index] > 0) {
      allocations.push_back(ResourceAllocation{ues[index].ueId, allocated[index]});
    }
  }
  return allocations;
}

}  // namespace ran
