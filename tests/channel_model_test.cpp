#include "ran/channel_model.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace {

ran::ChannelConfig traceConfig(std::vector<int> trace) {
  return ran::ChannelConfig{"trace", std::move(trace)};
}

ran::ChannelConfig randomWalkConfig(int minCqi, int maxCqi) {
  return ran::ChannelConfig{"random_walk", {}, minCqi, maxCqi};
}

TEST(ChannelModelTest, StaticCqiRemainsUnchanged) {
  ran::ChannelModel channel{ran::ChannelConfig{"static", {}, 1, 15}, 10, 2026};

  EXPECT_EQ(channel.cqiForSlot(0), 10);
  EXPECT_EQ(channel.cqiForSlot(1), 10);
  EXPECT_EQ(channel.cqiForSlot(100), 10);
}

TEST(ChannelModelTest, TraceReturnsValuesInOrder) {
  ran::ChannelModel channel{traceConfig({10, 8, 3, 9}), 10, 2026};

  EXPECT_EQ(channel.cqiForSlot(0), 10);
  EXPECT_EQ(channel.cqiForSlot(1), 8);
  EXPECT_EQ(channel.cqiForSlot(2), 3);
  EXPECT_EQ(channel.cqiForSlot(3), 9);
}

TEST(ChannelModelTest, TraceHoldsFinalValueAfterItEnds) {
  ran::ChannelModel channel{traceConfig({10, 8, 3}), 10, 2026};

  EXPECT_EQ(channel.cqiForSlot(2), 3);
  EXPECT_EQ(channel.cqiForSlot(3), 3);
  EXPECT_EQ(channel.cqiForSlot(100), 3);
}

TEST(ChannelModelTest, TraceModelsTemporaryPoorChannelAndRecovery) {
  ran::ChannelModel channel{traceConfig({11, 10, 3, 3, 4, 10, 11}), 11, 2026};

  EXPECT_EQ(channel.cqiForSlot(0), 11);
  EXPECT_EQ(channel.cqiForSlot(2), 3);
  EXPECT_EQ(channel.cqiForSlot(3), 3);
  EXPECT_EQ(channel.cqiForSlot(6), 11);
}

TEST(ChannelModelTest, RandomWalkStaysInsideConfiguredBounds) {
  ran::ChannelModel channel{randomWalkConfig(4, 12), 8, 2026};

  for (std::uint32_t slot = 0; slot < 500; ++slot) {
    const auto cqi = channel.cqiForSlot(slot);
    EXPECT_GE(cqi, 4);
    EXPECT_LE(cqi, 12);
  }
}

TEST(ChannelModelTest, SameSeedProducesSameRandomWalk) {
  ran::ChannelModel first{randomWalkConfig(1, 15), 8, 42};
  ran::ChannelModel second{randomWalkConfig(1, 15), 8, 42};

  for (std::uint32_t slot = 0; slot < 100; ++slot) {
    EXPECT_EQ(first.cqiForSlot(slot), second.cqiForSlot(slot)) << "slot " << slot;
  }
}

TEST(ChannelModelTest, BoundaryCqiValuesRemainClamped) {
  ran::ChannelModel lower{randomWalkConfig(1, 1), 1, 42};
  ran::ChannelModel upper{randomWalkConfig(15, 15), 15, 42};

  for (std::uint32_t slot = 0; slot < 50; ++slot) {
    EXPECT_EQ(lower.cqiForSlot(slot), 1);
    EXPECT_EQ(upper.cqiForSlot(slot), 15);
  }
}

}  // namespace
