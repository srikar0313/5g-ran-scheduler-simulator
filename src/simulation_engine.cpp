#include "ran/simulation_engine.hpp"

#include "ran/cqi_capacity.hpp"
#include "ran/metrics.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ran {
namespace {

constexpr double historicalThroughputPreviousWeight = 0.9;
constexpr double historicalThroughputCurrentWeight = 0.1;
constexpr std::uint32_t ueSeedMultiplier = 0x9E3779B9U;
constexpr std::uint32_t trafficSeedSalt = 0x85EBCA6BU;
constexpr std::uint32_t channelSeedSalt = 0xC2B2AE35U;

std::uint32_t seedForUe(std::uint32_t simulationSeed, int ueId, std::uint32_t salt) {
  return simulationSeed ^ (static_cast<std::uint32_t>(ueId) * ueSeedMultiplier) ^ salt;
}

std::uint64_t usefulResourceBlockDemand(const UeSchedulingView& ue) {
  if (ue.queuedBytes == 0) {
    return 0;
  }
  const auto bytesPerBlock = bytesPerResourceBlock(ue.cqi);
  return ue.queuedBytes / bytesPerBlock + (ue.queuedBytes % bytesPerBlock != 0 ? 1U : 0U);
}

void validateAllocations(std::span<const ResourceAllocation> allocations,
                         std::span<const UeSchedulingView> views,
                         std::uint32_t availableResourceBlocks) {
  std::unordered_set<int> allocatedUeIds;
  std::uint64_t totalResourceBlocks = 0;

  for (const auto& allocation : allocations) {
    const auto view = std::find_if(views.begin(), views.end(), [&](const auto& candidate) {
      return candidate.ueId == allocation.ueId;
    });
    if (view == views.end()) {
      throw std::runtime_error("Scheduler returned an allocation for unknown UE " +
                               std::to_string(allocation.ueId));
    }
    if (!allocatedUeIds.insert(allocation.ueId).second) {
      throw std::runtime_error("Scheduler returned duplicate allocations for UE " +
                               std::to_string(allocation.ueId));
    }
    if (allocation.resourceBlocks == 0) {
      throw std::runtime_error("Scheduler returned zero resource blocks for UE " +
                               std::to_string(allocation.ueId));
    }

    totalResourceBlocks += allocation.resourceBlocks;
    if (totalResourceBlocks > availableResourceBlocks) {
      throw std::runtime_error("Scheduler allocations exceed the slot capacity");
    }
    if (allocation.resourceBlocks > usefulResourceBlockDemand(*view)) {
      throw std::runtime_error("Scheduler allocation exceeds useful queued demand for UE " +
                               std::to_string(allocation.ueId));
    }
  }
}

}  // namespace

SimulationEngine::UeState::UeState(const UeConfig& config,
                                   std::uint32_t simulationSeed)
    : ue(config.id, config.priority, config.initialCqi),
      trafficGenerator(config.traffic,
                       seedForUe(simulationSeed, config.id, trafficSeedSalt)),
      channelModel(config.channel, config.initialCqi,
                   seedForUe(simulationSeed, config.id, channelSeedSalt)) {
  metrics.ueId = config.id;
}

SimulationEngine::SimulationEngine(SimulationConfig config,
                                   std::unique_ptr<IScheduler> scheduler,
                                   std::string schedulerName)
    : config_(std::move(config)),
      scheduler_(std::move(scheduler)),
      schedulerName_(std::move(schedulerName)) {
  if (!scheduler_) {
    throw std::invalid_argument("Simulation scheduler must not be null");
  }
  if (schedulerName_.empty()) {
    throw std::invalid_argument("Simulation scheduler name must not be empty");
  }
  if (config_.simulation.timeSlots == 0) {
    throw std::invalid_argument("Simulation must contain at least one time slot");
  }
  if (config_.simulation.resourceBlocksPerSlot == 0) {
    throw std::invalid_argument("Simulation must provide resource blocks per slot");
  }
  if (config_.ues.empty()) {
    throw std::invalid_argument("Simulation must contain at least one UE");
  }

  std::unordered_set<int> ueIds;
  ues_.reserve(config_.ues.size());
  for (const auto& ueConfig : config_.ues) {
    if (!ueIds.insert(ueConfig.id).second) {
      throw std::invalid_argument("Simulation UE IDs must be unique");
    }
    ues_.emplace_back(ueConfig, config_.simulation.randomSeed);
  }
}

SimulationResult SimulationEngine::run() {
  if (hasRun_) {
    throw std::logic_error("SimulationEngine instances can only be run once");
  }
  hasRun_ = true;

  SimulationResult result;
  result.schedulerName = schedulerName_;
  result.simulationSeed = config_.simulation.randomSeed;
  result.timeSlots = config_.simulation.timeSlots;
  result.totalAvailableResourceBlocks =
      static_cast<std::uint64_t>(config_.simulation.timeSlots) *
      config_.simulation.resourceBlocksPerSlot;
  result.perSlot.reserve(config_.simulation.timeSlots);

  for (std::uint32_t slot = 0; slot < config_.simulation.timeSlots; ++slot) {
    PerSlotMetrics slotMetrics;
    slotMetrics.slot = slot;

    for (auto& state : ues_) {
      auto packet = state.trafficGenerator.generate(slot);
      if (packet.has_value()) {
        ++state.metrics.generatedPackets;
        state.metrics.generatedBytes += packet->originalSizeBytes();
        slotMetrics.generatedBytes += packet->originalSizeBytes();
        state.ue.addPacket(std::move(*packet));
      }
    }

    for (auto& state : ues_) {
      const auto dropped = state.ue.removeExpiredPackets(slot);
      state.metrics.droppedPackets += dropped.packets;
      state.metrics.droppedBytes += dropped.bytes;
      slotMetrics.droppedBytes += dropped.bytes;
    }

    for (auto& state : ues_) {
      state.ue.updateCqi(state.channelModel.cqiForSlot(slot));
    }

    std::vector<UeSchedulingView> views;
    views.reserve(ues_.size());
    for (const auto& state : ues_) {
      views.emplace_back(state.ue.id(), state.ue.queuedBytes(), state.ue.currentCqi(),
                         state.ue.historicalAverageThroughput());
    }

    const auto allocations =
        scheduler_->schedule(views, config_.simulation.resourceBlocksPerSlot);
    validateAllocations(allocations, views, config_.simulation.resourceBlocksPerSlot);

    std::vector<std::uint64_t> transmittedBytes(ues_.size(), 0);
    for (const auto& allocation : allocations) {
      const auto index = static_cast<std::size_t>(std::distance(
          ues_.begin(), std::find_if(ues_.begin(), ues_.end(), [&](const auto& state) {
            return state.ue.id() == allocation.ueId;
          })));
      auto& state = ues_[index];
      const auto byteCapacity = static_cast<std::uint64_t>(allocation.resourceBlocks) *
                                bytesPerResourceBlock(state.ue.currentCqi());
      const auto transmission = state.ue.transmit(byteCapacity, slot);
      transmittedBytes[index] = transmission.bytes;
      state.metrics.completedPackets += transmission.completedPackets;
      state.metrics.transmittedBytes += transmission.bytes;
      state.metrics.allocatedResourceBlocks += allocation.resourceBlocks;
      state.completedPacketLatencies.insert(state.completedPacketLatencies.end(),
                                            transmission.completedPacketLatencies.begin(),
                                            transmission.completedPacketLatencies.end());
      slotMetrics.transmittedBytes += transmission.bytes;
      slotMetrics.allocatedResourceBlocks += allocation.resourceBlocks;
    }

    for (std::size_t index = 0; index < ues_.size(); ++index) {
      auto& ue = ues_[index].ue;
      const auto updatedThroughput =
          historicalThroughputPreviousWeight * ue.historicalAverageThroughput() +
          historicalThroughputCurrentWeight * static_cast<double>(transmittedBytes[index]);
      ue.updateHistoricalAverageThroughput(updatedThroughput);
    }

    result.totalGeneratedBytes += slotMetrics.generatedBytes;
    result.totalTransmittedBytes += slotMetrics.transmittedBytes;
    result.totalDroppedBytes += slotMetrics.droppedBytes;
    result.totalAllocatedResourceBlocks += slotMetrics.allocatedResourceBlocks;
    result.perSlot.push_back(slotMetrics);
  }

  result.perUe.reserve(ues_.size());
  for (const auto& state : ues_) {
    auto metrics = state.metrics;
    metrics.averageThroughputBytesPerSlot =
        static_cast<double>(metrics.transmittedBytes) /
        static_cast<double>(config_.simulation.timeSlots);
    metrics.averageCompletedPacketLatencySlots = average(state.completedPacketLatencies);
    metrics.percentile95CompletedPacketLatencySlots =
        nearestRankPercentile(state.completedPacketLatencies, 0.95);
    result.perUe.push_back(metrics);
  }

  result.resourceBlockUtilization =
      static_cast<double>(result.totalAllocatedResourceBlocks) /
      static_cast<double>(result.totalAvailableResourceBlocks);
  result.jainsFairnessIndex = jainsFairnessIndex(result.perUe);
  return result;
}

}  // namespace ran
