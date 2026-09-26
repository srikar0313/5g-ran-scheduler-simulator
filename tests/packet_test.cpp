#include "ran/packet.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>

namespace {

ran::Packet makePacket(std::uint64_t sizeBytes = 1'000, std::uint32_t arrivalSlot = 0,
                       std::uint32_t latencyBudgetSlots = 3) {
  return ran::Packet{1, sizeBytes, arrivalSlot, latencyBudgetSlots,
                     ran::TrafficCategory::Video};
}

TEST(PacketTest, PartiallyTransmitsRequestedBytes) {
  auto packet = makePacket();

  EXPECT_EQ(packet.transmit(400), 400);
  EXPECT_EQ(packet.remainingSizeBytes(), 600);
  EXPECT_FALSE(packet.isFullyTransmitted());
}

TEST(PacketTest, CompleteTransmissionMarksPacketFinished) {
  auto packet = makePacket();

  EXPECT_EQ(packet.transmit(1'000), 1'000);
  EXPECT_EQ(packet.remainingSizeBytes(), 0);
  EXPECT_TRUE(packet.isFullyTransmitted());
}

TEST(PacketTest, TransmissionDoesNotExceedRemainingSize) {
  auto packet = makePacket(200);

  EXPECT_EQ(packet.transmit(400), 200);
  EXPECT_EQ(packet.remainingSizeBytes(), 0);
  EXPECT_EQ(packet.transmit(100), 0);
}

TEST(PacketTest, ExpiresAtLatencyBudgetBoundary) {
  const auto packet = makePacket(1'000, 0, 3);

  EXPECT_FALSE(packet.isExpired(0));
  EXPECT_FALSE(packet.isExpired(2));
  EXPECT_TRUE(packet.isExpired(3));
  EXPECT_TRUE(packet.isExpired(4));
}

TEST(PacketTest, RejectsZeroSize) {
  EXPECT_THROW((ran::Packet{1, 0, 0, 3, ran::TrafficCategory::Voice}),
               std::invalid_argument);
}

TEST(PacketTest, RejectsZeroLatencyBudget) {
  EXPECT_THROW((ran::Packet{1, 100, 0, 0, ran::TrafficCategory::Voice}),
               std::invalid_argument);
}

}  // namespace
