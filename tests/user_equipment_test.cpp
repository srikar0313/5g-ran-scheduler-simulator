#include "ran/user_equipment.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

ran::Packet makePacket(std::uint64_t id, std::uint64_t sizeBytes,
                       std::uint32_t arrivalSlot = 0,
                       std::uint32_t latencyBudgetSlots = 3) {
  return ran::Packet{id, sizeBytes, arrivalSlot, latencyBudgetSlots,
                     ran::TrafficCategory::Download};
}

TEST(UserEquipmentTest, QueueIsInitiallyEmpty) {
  const ran::UserEquipment ue{1, 1, 10};

  EXPECT_TRUE(ue.queueEmpty());
  EXPECT_EQ(ue.queuedPacketCount(), 0);
  EXPECT_EQ(ue.queuedBytes(), 0);
}

TEST(UserEquipmentTest, AddsPacketsAndCalculatesQueuedBytes) {
  ran::UserEquipment ue{1, 1, 10};

  ue.addPacket(makePacket(1, 100));
  ue.addPacket(makePacket(2, 250));

  EXPECT_FALSE(ue.queueEmpty());
  EXPECT_EQ(ue.queuedPacketCount(), 2);
  EXPECT_EQ(ue.queuedBytes(), 350);
}

TEST(UserEquipmentTest, TransmitsPacketsInFifoOrder) {
  ran::UserEquipment ue{1, 1, 10};
  ue.addPacket(makePacket(1, 100));
  ue.addPacket(makePacket(2, 200));

  EXPECT_EQ(ue.transmit(150), 150);
  EXPECT_EQ(ue.queuedPacketCount(), 1);
  EXPECT_EQ(ue.queuedBytes(), 150);
  EXPECT_EQ(ue.totalTransmittedBytes(), 150);
}

TEST(UserEquipmentTest, SupportsPartialPacketTransmission) {
  ran::UserEquipment ue{1, 1, 10};
  ue.addPacket(makePacket(1, 100));

  EXPECT_EQ(ue.transmit(30), 30);
  EXPECT_EQ(ue.queuedPacketCount(), 1);
  EXPECT_EQ(ue.queuedBytes(), 70);
}

TEST(UserEquipmentTest, TransmittingFromEmptyQueueDoesNothing) {
  ran::UserEquipment ue{1, 1, 10};

  EXPECT_EQ(ue.transmit(500), 0);
  EXPECT_EQ(ue.totalTransmittedBytes(), 0);
}

TEST(UserEquipmentTest, RemovesAnAlreadyCompletedPacket) {
  auto packet = makePacket(1, 100);
  packet.transmit(100);
  ran::UserEquipment ue{1, 1, 10};
  ue.addPacket(packet);

  EXPECT_EQ(ue.transmit(0), 0);
  EXPECT_TRUE(ue.queueEmpty());
}

TEST(UserEquipmentTest, RemovesExpiredPacketsAndReportsDrops) {
  ran::UserEquipment ue{1, 1, 10};
  ue.addPacket(makePacket(1, 100, 0, 3));
  ue.addPacket(makePacket(2, 200, 2, 3));

  const auto dropped = ue.removeExpiredPackets(3);

  EXPECT_EQ(dropped.packets, 1);
  EXPECT_EQ(dropped.bytes, 100);
  EXPECT_EQ(ue.queuedPacketCount(), 1);
  EXPECT_EQ(ue.queuedBytes(), 200);
}

TEST(UserEquipmentTest, AccumulatesDroppedPacketAndByteCounters) {
  ran::UserEquipment ue{1, 1, 10};
  ue.addPacket(makePacket(1, 100, 0, 1));
  ue.addPacket(makePacket(2, 200, 0, 2));

  ue.removeExpiredPackets(1);
  ue.removeExpiredPackets(2);

  EXPECT_EQ(ue.totalDroppedPackets(), 2);
  EXPECT_EQ(ue.totalDroppedBytes(), 300);
}

TEST(UserEquipmentTest, RejectsInvalidCqi) {
  EXPECT_THROW((ran::UserEquipment{1, 1, 0}), std::invalid_argument);
  EXPECT_THROW((ran::UserEquipment{1, 1, 16}), std::invalid_argument);

  ran::UserEquipment ue{1, 1, 10};
  EXPECT_THROW(ue.updateCqi(0), std::invalid_argument);
}

TEST(UserEquipmentTest, UpdatesCqi) {
  ran::UserEquipment ue{1, 1, 10};

  ue.updateCqi(14);

  EXPECT_EQ(ue.currentCqi(), 14);
}

TEST(UserEquipmentTest, ValidatesHistoricalAverageThroughput) {
  EXPECT_THROW((ran::UserEquipment{1, 1, 10, -0.1}), std::invalid_argument);

  ran::UserEquipment ue{1, 1, 10};
  ue.updateHistoricalAverageThroughput(25.5);
  EXPECT_DOUBLE_EQ(ue.historicalAverageThroughput(), 25.5);
  EXPECT_THROW(ue.updateHistoricalAverageThroughput(-1.0), std::invalid_argument);
  EXPECT_THROW(ue.updateHistoricalAverageThroughput(std::numeric_limits<double>::infinity()),
               std::invalid_argument);
  EXPECT_DOUBLE_EQ(ue.historicalAverageThroughput(), 25.5);
}

TEST(UserEquipmentTest, RejectsNonPositivePriority) {
  EXPECT_THROW((ran::UserEquipment{1, 0, 10}), std::invalid_argument);
}

}  // namespace
