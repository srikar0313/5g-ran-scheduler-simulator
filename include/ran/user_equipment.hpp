#pragma once

#include "ran/packet.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>

namespace ran {

struct DropResult {
  std::uint64_t packets{};
  std::uint64_t bytes{};
};

class UserEquipment {
 public:
  UserEquipment(int id, int priority, int currentCqi,
                double historicalAverageThroughput = 0.0);

  [[nodiscard]] int id() const noexcept;
  [[nodiscard]] int priority() const noexcept;
  [[nodiscard]] int currentCqi() const noexcept;
  [[nodiscard]] bool queueEmpty() const noexcept;
  [[nodiscard]] std::size_t queuedPacketCount() const noexcept;
  [[nodiscard]] std::uint64_t queuedBytes() const noexcept;
  [[nodiscard]] double historicalAverageThroughput() const noexcept;
  [[nodiscard]] std::uint64_t totalTransmittedBytes() const noexcept;
  [[nodiscard]] std::uint64_t totalDroppedBytes() const noexcept;
  [[nodiscard]] std::uint64_t totalDroppedPackets() const noexcept;

  void addPacket(Packet packet);
  std::uint64_t transmit(std::uint64_t byteCapacity);
  DropResult removeExpiredPackets(std::uint32_t currentSlot);
  void updateCqi(int cqi);
  void updateHistoricalAverageThroughput(double throughput);

 private:
  int id_;
  int priority_;
  int currentCqi_;
  std::deque<Packet> packets_;
  double historicalAverageThroughput_;
  std::uint64_t totalTransmittedBytes_{0};
  std::uint64_t totalDroppedBytes_{0};
  std::uint64_t totalDroppedPackets_{0};
};

}  // namespace ran
