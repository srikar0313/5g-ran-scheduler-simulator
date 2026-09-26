#include "ran/round_robin_scheduler.hpp"

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

std::vector<UeSchedulingView> threeActiveUes() {
  return {{1, 5'000, 10}, {2, 5'000, 10}, {3, 5'000, 10}};
}

TEST(RoundRobinSchedulerTest, AllocatesEquallyWithEqualDemand) {
  ran::RoundRobinScheduler scheduler;
  const auto ues = threeActiveUes();

  const auto allocations = scheduler.schedule(ues, 6);

  expectAllocations(allocations, {{1, 2}, {2, 2}, {3, 2}});
}

TEST(RoundRobinSchedulerTest, AllocatesOneResourceBlockAtATime) {
  ran::RoundRobinScheduler scheduler;
  const auto ues = threeActiveUes();

  const auto allocations = scheduler.schedule(ues, 4);

  expectAllocations(allocations, {{1, 2}, {2, 1}, {3, 1}});
}

TEST(RoundRobinSchedulerTest, CursorContinuesAcrossCalls) {
  ran::RoundRobinScheduler scheduler;
  const auto ues = threeActiveUes();

  const auto first = scheduler.schedule(ues, 2);
  const auto second = scheduler.schedule(ues, 2);

  expectAllocations(first, {{1, 1}, {2, 1}});
  expectAllocations(second, {{1, 1}, {3, 1}});
}

TEST(RoundRobinSchedulerTest, SkipsEmptyQueues) {
  ran::RoundRobinScheduler scheduler;
  const std::vector<UeSchedulingView> ues{{1, 5'000, 10}, {2, 0, 10}, {3, 5'000, 10}};

  const auto allocations = scheduler.schedule(ues, 4);

  expectAllocations(allocations, {{1, 2}, {3, 2}});
}

TEST(RoundRobinSchedulerTest, AllEmptyQueuesProduceNoAllocations) {
  ran::RoundRobinScheduler scheduler;
  const std::vector<UeSchedulingView> ues{{1, 0, 10}, {2, 0, 10}};

  EXPECT_TRUE(scheduler.schedule(ues, 10).empty());
}

TEST(RoundRobinSchedulerTest, EmptyUeListProducesNoAllocations) {
  ran::RoundRobinScheduler scheduler;
  const std::vector<UeSchedulingView> ues;

  EXPECT_TRUE(scheduler.schedule(ues, 10).empty());
}

TEST(RoundRobinSchedulerTest, ZeroAvailableResourceBlocksProducesNoAllocations) {
  ran::RoundRobinScheduler scheduler;
  const auto ues = threeActiveUes();

  EXPECT_TRUE(scheduler.schedule(ues, 0).empty());
}

TEST(RoundRobinSchedulerTest, OneActiveUeReceivesOnlyUsefulResources) {
  ran::RoundRobinScheduler scheduler;
  const std::vector<UeSchedulingView> ues{{1, 1'500, 10}};

  const auto allocations = scheduler.schedule(ues, 5);

  expectAllocations(allocations, {{1, 2}});
}

TEST(RoundRobinSchedulerTest, MoreUesThanResourceBlocks) {
  ran::RoundRobinScheduler scheduler;
  const std::vector<UeSchedulingView> ues{
      {1, 5'000, 10}, {2, 5'000, 10}, {3, 5'000, 10}, {4, 5'000, 10}};

  const auto allocations = scheduler.schedule(ues, 2);

  expectAllocations(allocations, {{1, 1}, {2, 1}});
}

TEST(RoundRobinSchedulerTest, ExtraResourceBlocksRemainUnusedAfterDemandIsSatisfied) {
  ran::RoundRobinScheduler scheduler;
  const std::vector<UeSchedulingView> ues{{1, 100, 1}, {2, 100, 1}};

  const auto allocations = scheduler.schedule(ues, 10);

  expectAllocations(allocations, {{1, 1}, {2, 1}});
  EXPECT_EQ(totalAllocated(allocations), 2);
}

TEST(RoundRobinSchedulerTest, AllocationNeverExceedsSlotCapacity) {
  ran::RoundRobinScheduler scheduler;
  const auto ues = threeActiveUes();

  const auto allocations = scheduler.schedule(ues, 5);

  EXPECT_LE(totalAllocated(allocations), 5);
}

TEST(RoundRobinSchedulerTest, UeNeverReceivesMoreThanBacklogRequires) {
  ran::RoundRobinScheduler scheduler;
  const std::vector<UeSchedulingView> ues{{1, 1'001, 10}};

  const auto allocations = scheduler.schedule(ues, 100);

  expectAllocations(allocations, {{1, 2}});
}

TEST(RoundRobinSchedulerTest, ReturnedAllocationsFollowInputOrder) {
  ran::RoundRobinScheduler scheduler;
  const auto activeUes = threeActiveUes();
  static_cast<void>(scheduler.schedule(activeUes, 2));
  const std::vector<UeSchedulingView> ues{{1, 5'000, 10}, {2, 0, 10}, {3, 5'000, 10}};

  const auto allocations = scheduler.schedule(ues, 2);

  expectAllocations(allocations, {{1, 1}, {3, 1}});
}

TEST(RoundRobinSchedulerTest, EquivalentInstancesProduceDeterministicResults) {
  ran::RoundRobinScheduler first;
  ran::RoundRobinScheduler second;
  const auto ues = threeActiveUes();

  for (const auto available : {2U, 5U, 1U, 4U}) {
    expectAllocationsEqual(first.schedule(ues, available), second.schedule(ues, available));
  }
}

}  // namespace
