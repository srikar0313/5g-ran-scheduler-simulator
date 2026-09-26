#include "ran/packet.hpp"

#include <algorithm>
#include <stdexcept>

namespace ran {

Packet::Packet(std::uint64_t id, std::uint64_t sizeBytes, std::uint32_t arrivalSlot,
               std::uint32_t latencyBudgetSlots, TrafficCategory category)
    : id_(id),
      originalSizeBytes_(sizeBytes),
      remainingSizeBytes_(sizeBytes),
      arrivalSlot_(arrivalSlot),
      latencyBudgetSlots_(latencyBudgetSlots),
      trafficCategory_(category) {
  if (sizeBytes == 0) {
    throw std::invalid_argument("Packet size must be greater than zero");
  }
  if (latencyBudgetSlots == 0) {
    throw std::invalid_argument("Packet latency budget must be greater than zero");
  }
}

std::uint64_t Packet::id() const noexcept { return id_; }

std::uint64_t Packet::originalSizeBytes() const noexcept { return originalSizeBytes_; }

std::uint64_t Packet::remainingSizeBytes() const noexcept { return remainingSizeBytes_; }

std::uint32_t Packet::arrivalSlot() const noexcept { return arrivalSlot_; }

std::uint32_t Packet::latencyBudgetSlots() const noexcept { return latencyBudgetSlots_; }

TrafficCategory Packet::trafficCategory() const noexcept { return trafficCategory_; }

bool Packet::isFullyTransmitted() const noexcept { return remainingSizeBytes_ == 0; }

bool Packet::isExpired(std::uint32_t currentSlot) const noexcept {
  const auto expirySlot = static_cast<std::uint64_t>(arrivalSlot_) + latencyBudgetSlots_;
  return currentSlot >= expirySlot;
}

std::uint64_t Packet::transmit(std::uint64_t requestedBytes) noexcept {
  const auto transmittedBytes = std::min(requestedBytes, remainingSizeBytes_);
  remainingSizeBytes_ -= transmittedBytes;
  return transmittedBytes;
}

}  // namespace ran
