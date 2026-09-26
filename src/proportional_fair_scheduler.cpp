#include "ran/proportional_fair_scheduler.hpp"

#include "ran/cqi_capacity.hpp"
#include "ran/proportional_fair_metric.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
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

ProportionalFairScheduler::ProportionalFairScheduler(double epsilon) : epsilon_(epsilon) {
  if (!std::isfinite(epsilon) || epsilon <= 0.0) {
    throw std::invalid_argument("PF epsilon must be finite and greater than zero");
  }
}

std::vector<ResourceAllocation> ProportionalFairScheduler::schedule(
    std::span<const UeSchedulingView> ues, std::uint32_t availableResourceBlocks) {
  if (ues.empty() || availableResourceBlocks == 0) {
    return {};
  }

  std::vector<std::uint64_t> remainingDemand;
  remainingDemand.reserve(ues.size());
  for (const auto& ue : ues) {
    remainingDemand.push_back(requiredResourceBlocks(ue));
  }

  std::vector<std::uint32_t> allocated(ues.size(), 0);
  for (std::uint32_t resourceBlock = 0; resourceBlock < availableResourceBlocks;
       ++resourceBlock) {
    std::optional<std::size_t> bestIndex;
    double bestMetric = 0.0;

    for (std::size_t index = 0; index < ues.size(); ++index) {
      if (remainingDemand[index] == 0) {
        continue;
      }

      const auto metric = proportionalFairMetric(ues[index], epsilon_);
      const auto betterMetric = !bestIndex.has_value() || metric > bestMetric;
      const auto equalMetric = bestIndex.has_value() && metric == bestMetric;
      const auto fewerAllocations =
          equalMetric && allocated[index] < allocated[*bestIndex];
      const auto lowerUeId = equalMetric && allocated[index] == allocated[*bestIndex] &&
                             ues[index].ueId < ues[*bestIndex].ueId;

      if (betterMetric || fewerAllocations || lowerUeId) {
        bestIndex = index;
        bestMetric = metric;
      }
    }

    if (!bestIndex.has_value()) {
      break;
    }

    ++allocated[*bestIndex];
    --remainingDemand[*bestIndex];
  }

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
