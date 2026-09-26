#pragma once

#include "ran/traffic_category.hpp"

#include <cstdint>

namespace ran {

class Packet {
 public:
  Packet(std::uint64_t id, std::uint64_t sizeBytes, std::uint32_t arrivalSlot,
         std::uint32_t latencyBudgetSlots, TrafficCategory category);

  [[nodiscard]] std::uint64_t id() const noexcept;
  [[nodiscard]] std::uint64_t originalSizeBytes() const noexcept;
  [[nodiscard]] std::uint64_t remainingSizeBytes() const noexcept;
  [[nodiscard]] std::uint32_t arrivalSlot() const noexcept;
  [[nodiscard]] std::uint32_t latencyBudgetSlots() const noexcept;
  [[nodiscard]] TrafficCategory trafficCategory() const noexcept;
  [[nodiscard]] bool isFullyTransmitted() const noexcept;
  [[nodiscard]] bool isExpired(std::uint32_t currentSlot) const noexcept;

  std::uint64_t transmit(std::uint64_t requestedBytes) noexcept;

 private:
  std::uint64_t id_;
  std::uint64_t originalSizeBytes_;
  std::uint64_t remainingSizeBytes_;
  std::uint32_t arrivalSlot_;
  std::uint32_t latencyBudgetSlots_;
  TrafficCategory trafficCategory_;
};

}  // namespace ran
