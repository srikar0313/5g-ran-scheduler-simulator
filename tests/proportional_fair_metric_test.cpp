#include "ran/proportional_fair_metric.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace {

TEST(ProportionalFairMetricTest, CalculatesExpectedMetricWithNormalHistory) {
  const ran::UeSchedulingView ue{1, 5'000, 10, 500.0};

  EXPECT_DOUBLE_EQ(ran::proportionalFairMetric(ue), 2.0);
}

TEST(ProportionalFairMetricTest, HigherCqiIncreasesMetricWhenHistoryIsEqual) {
  const ran::UeSchedulingView lowerCqi{1, 5'000, 5, 500.0};
  const ran::UeSchedulingView higherCqi{2, 5'000, 10, 500.0};

  EXPECT_GT(ran::proportionalFairMetric(higherCqi),
            ran::proportionalFairMetric(lowerCqi));
}

TEST(ProportionalFairMetricTest, LowerHistoryIncreasesMetricWhenCqiIsEqual) {
  const ran::UeSchedulingView moreServed{1, 5'000, 10, 1'000.0};
  const ran::UeSchedulingView lessServed{2, 5'000, 10, 200.0};

  EXPECT_GT(ran::proportionalFairMetric(lessServed),
            ran::proportionalFairMetric(moreServed));
}

TEST(ProportionalFairMetricTest, ZeroHistoryUsesDefaultEpsilon) {
  const ran::UeSchedulingView ue{1, 5'000, 10, 0.0};

  EXPECT_DOUBLE_EQ(ran::proportionalFairMetric(ue), 1'000.0);
}

TEST(ProportionalFairMetricTest, SupportsCustomEpsilon) {
  const ran::UeSchedulingView ue{1, 5'000, 10, 0.0};

  EXPECT_DOUBLE_EQ(ran::proportionalFairMetric(ue, 100.0), 10.0);
}

TEST(ProportionalFairMetricTest, RejectsNegativeHistoricalThroughput) {
  const ran::UeSchedulingView ue{1, 5'000, 10, -1.0};

  EXPECT_THROW(static_cast<void>(ran::proportionalFairMetric(ue)), std::invalid_argument);
}

TEST(ProportionalFairMetricTest, RejectsInfiniteOrNanHistoricalThroughput) {
  const ran::UeSchedulingView infinite{
      1, 5'000, 10, std::numeric_limits<double>::infinity()};
  const ran::UeSchedulingView nan{1, 5'000, 10,
                                  std::numeric_limits<double>::quiet_NaN()};

  EXPECT_THROW(static_cast<void>(ran::proportionalFairMetric(infinite)),
               std::invalid_argument);
  EXPECT_THROW(static_cast<void>(ran::proportionalFairMetric(nan)), std::invalid_argument);
}

TEST(ProportionalFairMetricTest, RejectsZeroOrNegativeEpsilon) {
  const ran::UeSchedulingView ue{1, 5'000, 10, 500.0};

  EXPECT_THROW(static_cast<void>(ran::proportionalFairMetric(ue, 0.0)),
               std::invalid_argument);
  EXPECT_THROW(static_cast<void>(ran::proportionalFairMetric(ue, -1.0)),
               std::invalid_argument);
}

TEST(ProportionalFairMetricTest, RejectsInfiniteOrNanEpsilon) {
  const ran::UeSchedulingView ue{1, 5'000, 10, 500.0};

  EXPECT_THROW(static_cast<void>(ran::proportionalFairMetric(
                   ue, std::numeric_limits<double>::infinity())),
               std::invalid_argument);
  EXPECT_THROW(static_cast<void>(ran::proportionalFairMetric(
                   ue, std::numeric_limits<double>::quiet_NaN())),
               std::invalid_argument);
}

TEST(ProportionalFairMetricTest, RejectsInvalidCqi) {
  const ran::UeSchedulingView ue{1, 5'000, 0, 500.0};

  EXPECT_THROW(static_cast<void>(ran::proportionalFairMetric(ue)), std::invalid_argument);
}

}  // namespace
