#include "ran/traffic_generator.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace ran {
namespace {

void validateTrafficConfig(const TrafficConfig& config) {
  if (config.packetSizeBytes == 0) {
    throw std::invalid_argument("Traffic packet size must be greater than zero");
  }
  if (config.latencyBudgetSlots == 0) {
    throw std::invalid_argument("Traffic latency budget must be greater than zero");
  }
  if (config.model == "periodic") {
    if (config.periodSlots == 0) {
      throw std::invalid_argument("Periodic traffic period must be greater than zero");
    }
    return;
  }
  if (config.model == "bernoulli") {
    if (!std::isfinite(config.arrivalProbability) || config.arrivalProbability < 0.0 ||
        config.arrivalProbability > 1.0) {
      throw std::invalid_argument("Bernoulli arrival probability must be between 0 and 1");
    }
    return;
  }
  if (config.model != "none") {
    throw std::invalid_argument("Unsupported traffic model: " + config.model);
  }
}

}  // namespace

TrafficGenerator::TrafficGenerator(TrafficConfig config, std::uint32_t seed,
                                   std::uint64_t startingPacketId)
    : config_(std::move(config)), randomEngine_(seed), nextPacketId_(startingPacketId) {
  validateTrafficConfig(config_);
}

std::optional<Packet> TrafficGenerator::generate(std::uint32_t slot) {
  bool shouldGenerate = false;

  if (config_.model == "periodic") {
    shouldGenerate = slot % config_.periodSlots == 0;
  } else if (config_.model == "bernoulli") {
    if (config_.arrivalProbability == 1.0) {
      shouldGenerate = true;
    } else if (config_.arrivalProbability > 0.0) {
      // mt19937 has a standardized 32-bit output range. This threshold is a simple,
      // reproducible simulation abstraction that avoids library-specific distributions.
      constexpr double outputCount = static_cast<double>(std::mt19937::max()) + 1.0;
      const auto threshold =
          static_cast<std::uint64_t>(config_.arrivalProbability * outputCount);
      shouldGenerate = static_cast<std::uint64_t>(randomEngine_()) < threshold;
    }
  }

  if (!shouldGenerate) {
    return std::nullopt;
  }

  return Packet{nextPacketId_++, config_.packetSizeBytes, slot, config_.latencyBudgetSlots,
                config_.category};
}

}  // namespace ran
