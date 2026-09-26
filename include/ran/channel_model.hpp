#pragma once

#include "ran/simulation_config.hpp"

#include <cstdint>
#include <optional>
#include <random>

namespace ran {

class ChannelModel {
 public:
  ChannelModel(ChannelConfig config, int initialCqi, std::uint32_t seed);

  int cqiForSlot(std::uint32_t slot);

 private:
  void advanceRandomWalk();

  ChannelConfig config_;
  int currentCqi_;
  std::mt19937 randomEngine_;
  std::optional<std::uint32_t> lastRandomWalkSlot_;
};

}  // namespace ran
