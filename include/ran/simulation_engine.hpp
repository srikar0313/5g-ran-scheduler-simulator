#pragma once

#include "ran/channel_model.hpp"
#include "ran/i_scheduler.hpp"
#include "ran/simulation_config.hpp"
#include "ran/simulation_results.hpp"
#include "ran/traffic_generator.hpp"
#include "ran/user_equipment.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ran {

class SimulationEngine {
 public:
  SimulationEngine(SimulationConfig config, std::unique_ptr<IScheduler> scheduler,
                   std::string schedulerName);

  SimulationResult run();

 private:
  struct UeState {
    UeState(const UeConfig& config, std::uint32_t simulationSeed);

    UserEquipment ue;
    TrafficGenerator trafficGenerator;
    ChannelModel channelModel;
    PerUeMetrics metrics;
    std::vector<std::uint32_t> completedPacketLatencies;
  };

  SimulationConfig config_;
  std::unique_ptr<IScheduler> scheduler_;
  std::string schedulerName_;
  std::vector<UeState> ues_;
  bool hasRun_{false};
};

}  // namespace ran
