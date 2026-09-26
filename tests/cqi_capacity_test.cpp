#include "ran/cqi_capacity.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

TEST(CqiCapacityTest, CqiOneProvidesOneHundredBytes) {
  EXPECT_EQ(ran::bytesPerResourceBlock(1), 100);
}

TEST(CqiCapacityTest, CqiTenProvidesOneThousandBytes) {
  EXPECT_EQ(ran::bytesPerResourceBlock(10), 1'000);
}

TEST(CqiCapacityTest, CqiFifteenProvidesFifteenHundredBytes) {
  EXPECT_EQ(ran::bytesPerResourceBlock(15), 1'500);
}

TEST(CqiCapacityTest, RejectsCqiBelowOne) {
  EXPECT_THROW(static_cast<void>(ran::bytesPerResourceBlock(0)), std::invalid_argument);
}

TEST(CqiCapacityTest, RejectsCqiAboveFifteen) {
  EXPECT_THROW(static_cast<void>(ran::bytesPerResourceBlock(16)), std::invalid_argument);
}

TEST(CqiCapacityTest, CapacityIncreasesWithCqi) {
  for (int cqi = 2; cqi <= 15; ++cqi) {
    EXPECT_GT(ran::bytesPerResourceBlock(cqi), ran::bytesPerResourceBlock(cqi - 1));
  }
}

}  // namespace
