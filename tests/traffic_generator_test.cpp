#include "ran/traffic_generator.hpp"

#include <gtest/gtest.h>

#include <cstdint>

namespace {

ran::TrafficConfig periodicConfig(std::uint32_t periodSlots = 3) {
  return ran::TrafficConfig{"periodic", 1'000, 5, periodSlots, 0.0,
                            ran::TrafficCategory::Video};
}

ran::TrafficConfig bernoulliConfig(double probability) {
  return ran::TrafficConfig{"bernoulli", 500, 4, 0, probability,
                            ran::TrafficCategory::Voice};
}

TEST(TrafficGeneratorTest, GeneratesAtExpectedPeriodicSlots) {
  ran::TrafficGenerator generator{periodicConfig(), 10};

  for (std::uint32_t slot = 0; slot < 10; ++slot) {
    EXPECT_EQ(generator.generate(slot).has_value(), slot % 3 == 0) << "slot " << slot;
  }
}

TEST(TrafficGeneratorTest, DoesNotGenerateBetweenPeriodicSlots) {
  ran::TrafficGenerator generator{periodicConfig(), 10};

  EXPECT_TRUE(generator.generate(0).has_value());
  EXPECT_FALSE(generator.generate(1).has_value());
  EXPECT_FALSE(generator.generate(2).has_value());
  EXPECT_TRUE(generator.generate(3).has_value());
}

TEST(TrafficGeneratorTest, GeneratedPacketUsesConfiguredProperties) {
  ran::TrafficGenerator generator{periodicConfig(), 10, 42};

  const auto packet = generator.generate(0);

  ASSERT_TRUE(packet.has_value());
  EXPECT_EQ(packet->id(), 42);
  EXPECT_EQ(packet->originalSizeBytes(), 1'000);
  EXPECT_EQ(packet->remainingSizeBytes(), 1'000);
  EXPECT_EQ(packet->arrivalSlot(), 0);
  EXPECT_EQ(packet->latencyBudgetSlots(), 5);
  EXPECT_EQ(packet->trafficCategory(), ran::TrafficCategory::Video);
}

TEST(TrafficGeneratorTest, GeneratedPacketIdsIncrement) {
  ran::TrafficGenerator generator{periodicConfig(2), 10, 100};

  const auto first = generator.generate(0);
  const auto second = generator.generate(2);

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(first->id(), 100);
  EXPECT_EQ(second->id(), 101);
}

TEST(TrafficGeneratorTest, NoneModelNeverGenerates) {
  const ran::TrafficConfig config{"none", 1, 1, 0, 0.0,
                                  ran::TrafficCategory::Download};
  ran::TrafficGenerator generator{config, 10, 25};

  for (std::uint32_t slot = 0; slot < 20; ++slot) {
    EXPECT_FALSE(generator.generate(slot).has_value());
  }
}

TEST(TrafficGeneratorTest, ZeroBernoulliProbabilityNeverGenerates) {
  ran::TrafficGenerator generator{bernoulliConfig(0.0), 10};

  for (std::uint32_t slot = 0; slot < 20; ++slot) {
    EXPECT_FALSE(generator.generate(slot).has_value());
  }
}

TEST(TrafficGeneratorTest, OneBernoulliProbabilityAlwaysGenerates) {
  ran::TrafficGenerator generator{bernoulliConfig(1.0), 10};

  for (std::uint32_t slot = 0; slot < 20; ++slot) {
    EXPECT_TRUE(generator.generate(slot).has_value());
  }
}

TEST(TrafficGeneratorTest, SameSeedProducesSameBernoulliArrivals) {
  ran::TrafficGenerator first{bernoulliConfig(0.4), 2026};
  ran::TrafficGenerator second{bernoulliConfig(0.4), 2026};
  std::uint32_t generatedCount = 0;

  for (std::uint32_t slot = 0; slot < 100; ++slot) {
    const auto firstArrival = first.generate(slot).has_value();
    const auto secondArrival = second.generate(slot).has_value();
    EXPECT_EQ(firstArrival, secondArrival) << "slot " << slot;
    generatedCount += firstArrival ? 1U : 0U;
  }

  EXPECT_GT(generatedCount, 0);
  EXPECT_LT(generatedCount, 100);
}

TEST(TrafficGeneratorTest, BernoulliPacketUsesCurrentArrivalSlot) {
  ran::TrafficGenerator generator{bernoulliConfig(1.0), 10};

  const auto packet = generator.generate(7);

  ASSERT_TRUE(packet.has_value());
  EXPECT_EQ(packet->arrivalSlot(), 7);
}

}  // namespace
