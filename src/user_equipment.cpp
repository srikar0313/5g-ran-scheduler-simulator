#include "ran/user_equipment.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace ran {
namespace {

void validateCqi(int cqi) {
  if (cqi < 1 || cqi > 15) {
    throw std::invalid_argument("CQI must be between 1 and 15");
  }
}

void validateThroughput(double throughput) {
  if (!std::isfinite(throughput) || throughput < 0.0) {
    throw std::invalid_argument(
        "Historical average throughput must be finite and not negative");
  }
}

}  // namespace

UserEquipment::UserEquipment(int id, int priority, int currentCqi,
                             double historicalAverageThroughput)
    : id_(id),
      priority_(priority),
      currentCqi_(currentCqi),
      historicalAverageThroughput_(historicalAverageThroughput) {
  if (priority <= 0) {
    throw std::invalid_argument("UE priority must be positive");
  }
  validateCqi(currentCqi);
  validateThroughput(historicalAverageThroughput);
}

int UserEquipment::id() const noexcept { return id_; }

int UserEquipment::priority() const noexcept { return priority_; }

int UserEquipment::currentCqi() const noexcept { return currentCqi_; }

bool UserEquipment::queueEmpty() const noexcept { return packets_.empty(); }

std::size_t UserEquipment::queuedPacketCount() const noexcept { return packets_.size(); }

std::uint64_t UserEquipment::queuedBytes() const noexcept {
  std::uint64_t totalBytes = 0;
  for (const auto& packet : packets_) {
    totalBytes += packet.remainingSizeBytes();
  }
  return totalBytes;
}

double UserEquipment::historicalAverageThroughput() const noexcept {
  return historicalAverageThroughput_;
}

std::uint64_t UserEquipment::totalTransmittedBytes() const noexcept {
  return totalTransmittedBytes_;
}

std::uint64_t UserEquipment::totalDroppedBytes() const noexcept { return totalDroppedBytes_; }

std::uint64_t UserEquipment::totalDroppedPackets() const noexcept {
  return totalDroppedPackets_;
}

void UserEquipment::addPacket(Packet packet) { packets_.push_back(std::move(packet)); }

TransmissionResult UserEquipment::transmit(std::uint64_t byteCapacity,
                                           std::uint32_t currentSlot) {
  TransmissionResult result;

  while (!packets_.empty()) {
    auto& packet = packets_.front();
    if (packet.isFullyTransmitted()) {
      ++result.completedPackets;
      result.completedPacketLatencies.push_back(currentSlot - packet.arrivalSlot() + 1);
      packets_.pop_front();
      continue;
    }
    if (byteCapacity == 0) {
      break;
    }

    const auto packetBytes = packet.transmit(byteCapacity);
    result.bytes += packetBytes;
    byteCapacity -= packetBytes;

    if (packet.isFullyTransmitted()) {
      ++result.completedPackets;
      result.completedPacketLatencies.push_back(currentSlot - packet.arrivalSlot() + 1);
      packets_.pop_front();
    }
  }

  totalTransmittedBytes_ += result.bytes;
  return result;
}

DropResult UserEquipment::removeExpiredPackets(std::uint32_t currentSlot) {
  DropResult result;
  auto packet = packets_.begin();

  while (packet != packets_.end()) {
    if (packet->isExpired(currentSlot)) {
      ++result.packets;
      result.bytes += packet->remainingSizeBytes();
      packet = packets_.erase(packet);
    } else {
      ++packet;
    }
  }

  totalDroppedPackets_ += result.packets;
  totalDroppedBytes_ += result.bytes;
  return result;
}

void UserEquipment::updateCqi(int cqi) {
  validateCqi(cqi);
  currentCqi_ = cqi;
}

void UserEquipment::updateHistoricalAverageThroughput(double throughput) {
  validateThroughput(throughput);
  historicalAverageThroughput_ = throughput;
}

}  // namespace ran
