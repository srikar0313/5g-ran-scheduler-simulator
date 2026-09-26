#include "ran/proportional_fair_scheduler.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <vector>

namespace {

using ran::ResourceAllocation;
using ran::UeSchedulingView;

void expectAllocations(const std::vector<ResourceAllocation>& actual,
                       std::initializer_list<ResourceAllocation> expected) {
  ASSERT_EQ(actual.size(), expected.size());
  auto expectedAllocation = expected.begin();
  for (std::size_t index = 0; index < actual.size(); ++index, ++expectedAllocation) {
    EXPECT_EQ(actual[index].ueId, expectedAllocation->ueId);
    EXPECT_EQ(actual[index].resourceBlocks, expectedAllocation->resourceBlocks);
  }
}

void expectAllocationsEqual(const std::vector<ResourceAllocation>& first,
                            const std::vector<ResourceAllocation>& second) {
  ASSERT_EQ(first.size(), second.size());
  for (std::size_t index = 0; index < first.size(); ++index) {
    EXPECT_EQ(first[index].ueId, second[index].ueId);
    EXPECT_EQ(first[index].resourceBlocks, second[index].resourceBlocks);
  }
}

std::uint32_t totalAllocated(const std::vector<ResourceAllocation>& allocations) {
  std::uint32_t total = 0;
  for (const auto& allocation : allocations) {
    total += allocation.resourceBlocks;
  }
  return total;
}

TEST(ProportionalFairSchedulerTest, BetterChannelWinsWhenHistoryIsEqual) {
  ran::ProportionalFairScheduler scheduler;
  const std::vector<UeSchedulingView> ues{
      {1, 5'000, 10, 500.0}, {2, 5'000, 5, 500.0}};

  const auto allocations = scheduler.schedule(ues, 1);

  expectAllocations(allocations, {{1, 1}});
}

TEST(ProportionalFairSchedulerTest, UnderservedUeWinsWhenCqiIsEqual) {
  ran::ProportionalFairScheduler scheduler;
  const std::vector<UeSchedulingView> ues{
      {1, 5'000, 10, 1'000.0}, {2, 5'000, 10, 200.0}};

  const auto allocations = scheduler.schedule(ues, 1);

  expectAllocations(allocations, {{2, 1}});
}

TEST(ProportionalFairSchedulerTest, HighestMetricReceivesFirstResourceBlock) {
  ran::ProportionalFairScheduler scheduler;
  const std::vector<UeSchedulingView> ues{{1, 5'000, 5, 500.0},
                                           {2, 5'000, 8, 200.0},
                                           {3, 5'000, 15, 1'000.0}};

  const auto allocations = scheduler.schedule(ues, 1);

  expectAllocations(allocations, {{2, 1}});
}

TEST(ProportionalFairSchedulerTest, EqualScoresShareResourceBlocks) {
  ran::ProportionalFairScheduler scheduler;
  const std::vector<UeSchedulingView> ues{
      {1, 5'000, 10, 500.0}, {2, 5'000, 10, 500.0}};

  const auto allocations = scheduler.schedule(ues, 4);

  expectAllocations(allocations, {{1, 2}, {2, 2}});
}

TEST(ProportionalFairSchedulerTest, LowerUeIdResolvesCompleteTie) {
  ran::ProportionalFairScheduler scheduler;
  const std::vector<UeSchedulingView> ues{
      {2, 5'000, 10, 500.0}, {1, 5'000, 10, 500.0}};

  const auto allocations = scheduler.schedule(ues, 1);

  expectAllocations(allocations, {{1, 1}});
}

TEST(ProportionalFairSchedulerTest, SkipsEmptyQueues) {
  ran::ProportionalFairScheduler scheduler;
  const std::vector<UeSchedulingView> ues{
      {1, 0, 15, 0.0}, {2, 1'000, 5, 500.0}};

  const auto allocations = scheduler.schedule(ues, 1);

  expectAllocations(allocations, {{2, 1}});
}

TEST(ProportionalFairSchedulerTest, EmptyUeListReturnsNoAllocations) {
  ran::ProportionalFairScheduler scheduler;
  const std::vector<UeSchedulingView> ues;

  EXPECT_TRUE(scheduler.schedule(ues, 10).empty());
}

TEST(ProportionalFairSchedulerTest, ZeroAvailableResourceBlocksReturnsNoAllocations) {
  ran::ProportionalFairScheduler scheduler;
  const std::vector<UeSchedulingView> ues{{1, 5'000, 10, 500.0}};

  EXPECT_TRUE(scheduler.schedule(ues, 0).empty());
}

TEST(ProportionalFairSchedulerTest, AllEmptyQueuesReturnNoAllocations) {
  ran::ProportionalFairScheduler scheduler;
  const std::vector<UeSchedulingView> ues{{1, 0, 10, 500.0}, {2, 0, 5, 100.0}};

  EXPECT_TRUE(scheduler.schedule(ues, 10).empty());
}

TEST(ProportionalFairSchedulerTest, OneActiveUeReceivesOnlyUsefulResources) {
  ran::ProportionalFairScheduler scheduler;
  const std::vector<UeSchedulingView> ues{{1, 1'500, 10, 500.0}};

  const auto allocations = scheduler.schedule(ues, 5);

  expectAllocations(allocations, {{1, 2}});
}

TEST(ProportionalFairSchedulerTest, AllocationNeverExceedsAvailableResourceBlocks) {
  ran::ProportionalFairScheduler scheduler;
  const std::vector<UeSchedulingView> ues{
      {1, 5'000, 10, 500.0}, {2, 5'000, 5, 500.0}};

  const auto allocations = scheduler.schedule(ues, 3);

  EXPECT_LE(totalAllocated(allocations), 3);
}

TEST(ProportionalFairSchedulerTest, AllocationNeverExceedsUsefulBacklogDemand) {
  ran::ProportionalFairScheduler scheduler;
  const std::vector<UeSchedulingView> ues{{1, 1'001, 10, 500.0}};

  const auto allocations = scheduler.schedule(ues, 100);

  expectAllocations(allocations, {{1, 2}});
}

TEST(ProportionalFairSchedulerTest, ExtraResourceBlocksRemainUnused) {
  ran::ProportionalFairScheduler scheduler;
  const std::vector<UeSchedulingView> ues{
      {1, 100, 1, 100.0}, {2, 100, 1, 100.0}};

  const auto allocations = scheduler.schedule(ues, 10);

  expectAllocations(allocations, {{1, 1}, {2, 1}});
  EXPECT_EQ(totalAllocated(allocations), 2);
}

TEST(ProportionalFairSchedulerTest, ReturnedAllocationsFollowInputOrder) {
  ran::ProportionalFairScheduler scheduler;
  const std::vector<UeSchedulingView> ues{{30, 5'000, 10, 500.0},
                                           {10, 5'000, 10, 500.0},
                                           {20, 5'000, 10, 500.0}};

  const auto allocations = scheduler.schedule(ues, 3);

  expectAllocations(allocations, {{30, 1}, {10, 1}, {20, 1}});
}

TEST(ProportionalFairSchedulerTest, EquivalentInstancesProduceDeterministicResults) {
  ran::ProportionalFairScheduler first;
  ran::ProportionalFairScheduler second;
  const std::vector<UeSchedulingView> ues{{1, 5'000, 10, 500.0},
                                           {2, 5'000, 8, 200.0},
                                           {3, 5'000, 15, 1'000.0}};

  for (const auto available : {1U, 3U, 8U}) {
    expectAllocationsEqual(first.schedule(ues, available),
                           second.schedule(ues, available));
  }
}

}  // namespace
