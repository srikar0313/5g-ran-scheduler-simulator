#pragma once

#include "ran/traffic_category.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ran {

struct SimulationSettings {
  std::uint32_t timeSlots;
  double slotDurationMs;
  std::uint32_t resourceBlocksPerSlot;
  std::uint32_t randomSeed;
};

struct TrafficConfig {
  std::string model;
  std::uint64_t packetSizeBytes;
  std::uint32_t latencyBudgetSlots;
  std::uint32_t periodSlots{0};
  double arrivalProbability{0.0};
  TrafficCategory category;
};

struct ChannelConfig {
  std::string model;
  std::vector<int> cqiTrace;
  int minCqi{1};
  int maxCqi{15};
};

struct UeConfig {
  int id;
  int priority;
  int initialCqi;
  TrafficConfig traffic;
  ChannelConfig channel;
};

struct SimulationConfig {
  SimulationSettings simulation;
  std::vector<UeConfig> ues;
};

}  // namespace ran
