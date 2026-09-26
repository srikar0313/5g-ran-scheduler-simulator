#pragma once

#include "ran/packet.hpp"
#include "ran/simulation_config.hpp"

#include <cstdint>
#include <optional>
#include <random>

namespace ran {

class TrafficGenerator {
 public:
  TrafficGenerator(TrafficConfig config, std::uint32_t seed,
                   std::uint64_t startingPacketId = 1);

  std::optional<Packet> generate(std::uint32_t slot);

 private:
  TrafficConfig config_;
  std::mt19937 randomEngine_;
  std::uint64_t nextPacketId_;
};

}  // namespace ran
