#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ran {

struct PerUeMetrics {
  int ueId{};
  std::uint64_t generatedPackets{};
  std::uint64_t generatedBytes{};
  std::uint64_t completedPackets{};
  std::uint64_t transmittedBytes{};
  std::uint64_t droppedPackets{};
  std::uint64_t droppedBytes{};
  std::uint64_t allocatedResourceBlocks{};
  double averageThroughputBytesPerSlot{};
  double averageCompletedPacketLatencySlots{};
  double percentile95CompletedPacketLatencySlots{};
};

struct PerSlotMetrics {
  std::uint32_t slot{};
  std::uint64_t generatedBytes{};
  std::uint64_t transmittedBytes{};
  std::uint64_t droppedBytes{};
  std::uint64_t allocatedResourceBlocks{};
};

struct SimulationResult {
  std::string schedulerName;
  std::uint32_t simulationSeed{};
  std::uint32_t timeSlots{};
  std::uint64_t totalGeneratedBytes{};
  std::uint64_t totalTransmittedBytes{};
  std::uint64_t totalDroppedBytes{};
  std::uint64_t totalAllocatedResourceBlocks{};
  std::uint64_t totalAvailableResourceBlocks{};
  double resourceBlockUtilization{};
  double jainsFairnessIndex{};
  std::vector<PerUeMetrics> perUe;
  std::vector<PerSlotMetrics> perSlot;
};

}  // namespace ran
