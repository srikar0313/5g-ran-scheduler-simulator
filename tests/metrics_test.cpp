#include "ran/metrics.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

TEST(MetricsTest, AverageIsZeroForNoValues) {
  const std::vector<std::uint32_t> values;

  EXPECT_DOUBLE_EQ(ran::average(values), 0.0);
}

TEST(MetricsTest, CalculatesAverage) {
  const std::vector<std::uint32_t> values{1, 2, 6};

  EXPECT_DOUBLE_EQ(ran::average(values), 3.0);
}

TEST(MetricsTest, NearestRankPercentileSortsValues) {
  const std::vector<std::uint32_t> values{100, 2, 4, 1, 3};

  EXPECT_DOUBLE_EQ(ran::nearestRankPercentile(values, 0.5), 3.0);
  EXPECT_DOUBLE_EQ(ran::nearestRankPercentile(values, 0.95), 100.0);
}

TEST(MetricsTest, PercentileIsZeroForNoValues) {
  const std::vector<std::uint32_t> values;

  EXPECT_DOUBLE_EQ(ran::nearestRankPercentile(values, 0.95), 0.0);
}

TEST(MetricsTest, RejectsInvalidPercentile) {
  const std::vector<std::uint32_t> values{1};

  EXPECT_THROW(ran::nearestRankPercentile(values, 0.0), std::invalid_argument);
  EXPECT_THROW(ran::nearestRankPercentile(values, 1.1), std::invalid_argument);
}

TEST(MetricsTest, JainIndexIsOneForEqualThroughput) {
  std::vector<ran::PerUeMetrics> metrics(3);
  for (auto& ue : metrics) {
    ue.averageThroughputBytesPerSlot = 50.0;
  }

  EXPECT_DOUBLE_EQ(ran::jainsFairnessIndex(metrics), 1.0);
}

TEST(MetricsTest, JainIndexReflectsUnequalThroughput) {
  std::vector<ran::PerUeMetrics> metrics(2);
  metrics[0].averageThroughputBytesPerSlot = 10.0;
  metrics[1].averageThroughputBytesPerSlot = 0.0;

  EXPECT_DOUBLE_EQ(ran::jainsFairnessIndex(metrics), 0.5);
}

TEST(MetricsTest, JainIndexIsZeroWhenAllThroughputIsZero) {
  const std::vector<ran::PerUeMetrics> metrics(2);

  EXPECT_DOUBLE_EQ(ran::jainsFairnessIndex(metrics), 0.0);
}

}  // namespace
