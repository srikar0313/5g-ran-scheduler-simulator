#include "ran/channel_model.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ran {
namespace {

bool isValidCqi(int cqi) { return cqi >= 1 && cqi <= 15; }

void validateChannelConfig(const ChannelConfig& config, int initialCqi) {
  if (!isValidCqi(initialCqi)) {
    throw std::invalid_argument("Initial CQI must be between 1 and 15");
  }
  if (config.model == "static") {
    return;
  }
  if (config.model == "trace") {
    if (config.cqiTrace.empty()) {
      throw std::invalid_argument("CQI trace must not be empty");
    }
    for (const auto cqi : config.cqiTrace) {
      if (!isValidCqi(cqi)) {
        throw std::invalid_argument("CQI trace values must be between 1 and 15");
      }
    }
    return;
  }
  if (config.model == "random_walk") {
    if (!isValidCqi(config.minCqi) || !isValidCqi(config.maxCqi)) {
      throw std::invalid_argument("Random-walk CQI limits must be between 1 and 15");
    }
    if (config.minCqi > config.maxCqi) {
      throw std::invalid_argument("Random-walk minimum CQI must not exceed maximum CQI");
    }
    if (initialCqi < config.minCqi || initialCqi > config.maxCqi) {
      throw std::invalid_argument("Initial CQI must be inside the random-walk range");
    }
    return;
  }
  throw std::invalid_argument("Unsupported channel model: " + config.model);
}

}  // namespace

ChannelModel::ChannelModel(ChannelConfig config, int initialCqi, std::uint32_t seed)
    : config_(std::move(config)), currentCqi_(initialCqi), randomEngine_(seed) {
  validateChannelConfig(config_, initialCqi);
}

int ChannelModel::cqiForSlot(std::uint32_t slot) {
  if (config_.model == "static") {
    return currentCqi_;
  }
  if (config_.model == "trace") {
    const auto index = static_cast<std::size_t>(slot);
    if (index < config_.cqiTrace.size()) {
      return config_.cqiTrace[index];
    }
    return config_.cqiTrace.back();
  }

  if (!lastRandomWalkSlot_.has_value()) {
    lastRandomWalkSlot_ = 0;
  }
  if (slot < *lastRandomWalkSlot_) {
    throw std::invalid_argument("Random-walk CQI slots must be requested in order");
  }
  while (*lastRandomWalkSlot_ < slot) {
    advanceRandomWalk();
    ++(*lastRandomWalkSlot_);
  }
  return currentCqi_;
}

void ChannelModel::advanceRandomWalk() {
  // Modulo three is sufficient for this small educational model: 0, 1, and 2 map
  // directly to CQI steps -1, 0, and +1.
  const auto step = static_cast<int>(randomEngine_() % 3) - 1;
  currentCqi_ = std::clamp(currentCqi_ + step, config_.minCqi, config_.maxCqi);
}

}  // namespace ran
