#include "ran/proportional_fair_scheduler.hpp"
#include "ran/round_robin_scheduler.hpp"
#include "ran/simulation_engine.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

ran::TrafficConfig periodicTraffic(std::uint64_t packetSize = 100,
                                   std::uint32_t latencyBudget = 10,
                                   std::uint32_t period = 1) {
  return {"periodic", packetSize, latencyBudget, period, 0.0,
          ran::TrafficCategory::Download};
}

ran::TrafficConfig noTraffic() {
  return {"none", 1, 1, 0, 0.0, ran::TrafficCategory::Download};
}

ran::UeConfig makeUe(int id, int cqi, ran::TrafficConfig traffic) {
  return {id, 1, cqi, std::move(traffic), {"static", {}, 1, 15}};
}

ran::SimulationConfig makeConfig(std::uint32_t slots, std::uint32_t resourceBlocks,
                                 std::vector<ran::UeConfig> ues,
                                 std::uint32_t seed = 42) {
  return {{slots, 1.0, resourceBlocks, seed}, std::move(ues)};
}

class EmptyScheduler final : public ran::IScheduler {
 public:
  std::vector<ran::ResourceAllocation> schedule(
      std::span<const ran::UeSchedulingView>, std::uint32_t) override {
    return {};
  }
};

class RecordingScheduler final : public ran::IScheduler {
 public:
  std::vector<ran::ResourceAllocation> schedule(
      std::span<const ran::UeSchedulingView> ues, std::uint32_t) override {
    historicalThroughputs.push_back(ues.front().historicalAverageThroughput);
    return {{ues.front().ueId, 1}};
  }

  std::vector<double> historicalThroughputs;
};

class FixedScheduler final : public ran::IScheduler {
 public:
  explicit FixedScheduler(std::vector<ran::ResourceAllocation> allocations)
      : allocations_(std::move(allocations)) {}

  std::vector<ran::ResourceAllocation> schedule(
      std::span<const ran::UeSchedulingView>, std::uint32_t) override {
    return allocations_;
  }

 private:
  std::vector<ran::ResourceAllocation> allocations_;
};

TEST(SimulationEngineTest, RunsDeterministicSingleUeSimulation) {
  auto config = makeConfig(3, 1, {makeUe(1, 5, periodicTraffic(500))});
  ran::SimulationEngine engine{std::move(config),
                               std::make_unique<ran::RoundRobinScheduler>(),
                               "round-robin"};

  const auto result = engine.run();

  ASSERT_EQ(result.perUe.size(), 1);
  EXPECT_EQ(result.totalGeneratedBytes, 1'500);
  EXPECT_EQ(result.totalTransmittedBytes, 1'500);
  EXPECT_EQ(result.totalDroppedBytes, 0);
  EXPECT_EQ(result.totalAllocatedResourceBlocks, 3);
  EXPECT_EQ(result.totalAvailableResourceBlocks, 3);
  EXPECT_DOUBLE_EQ(result.resourceBlockUtilization, 1.0);
  EXPECT_DOUBLE_EQ(result.jainsFairnessIndex, 1.0);
  EXPECT_EQ(result.perUe[0].completedPackets, 3);
  EXPECT_DOUBLE_EQ(result.perUe[0].averageThroughputBytesPerSlot, 500.0);
  EXPECT_DOUBLE_EQ(result.perUe[0].averageCompletedPacketLatencySlots, 1.0);
  EXPECT_DOUBLE_EQ(result.perUe[0].percentile95CompletedPacketLatencySlots, 1.0);
}

TEST(SimulationEngineTest, NoTrafficProducesZeroMetrics) {
  auto config = makeConfig(4, 2, {makeUe(1, 8, noTraffic())});
  ran::SimulationEngine engine{std::move(config),
                               std::make_unique<ran::RoundRobinScheduler>(),
                               "round-robin"};

  const auto result = engine.run();

  EXPECT_EQ(result.totalGeneratedBytes, 0);
  EXPECT_EQ(result.totalTransmittedBytes, 0);
  EXPECT_EQ(result.totalDroppedBytes, 0);
  EXPECT_EQ(result.totalAllocatedResourceBlocks, 0);
  EXPECT_DOUBLE_EQ(result.resourceBlockUtilization, 0.0);
  EXPECT_DOUBLE_EQ(result.jainsFairnessIndex, 0.0);
  EXPECT_DOUBLE_EQ(result.perUe[0].averageCompletedPacketLatencySlots, 0.0);
  EXPECT_DOUBLE_EQ(result.perUe[0].percentile95CompletedPacketLatencySlots, 0.0);
}

TEST(SimulationEngineTest, DropsPacketsAtTheirLatencyDeadline) {
  auto config = makeConfig(2, 1, {makeUe(1, 1, periodicTraffic(1'000, 1, 100))});
  ran::SimulationEngine engine{std::move(config), std::make_unique<EmptyScheduler>(),
                               "empty"};

  const auto result = engine.run();

  EXPECT_EQ(result.perUe[0].generatedPackets, 1);
  EXPECT_EQ(result.perUe[0].droppedPackets, 1);
  EXPECT_EQ(result.perUe[0].droppedBytes, 1'000);
  EXPECT_EQ(result.perSlot[1].droppedBytes, 1'000);
}

TEST(SimulationEngineTest, UpdatesHistoricalThroughputAfterEachSlot) {
  auto scheduler = std::make_unique<RecordingScheduler>();
  auto* recorder = scheduler.get();
  auto config = makeConfig(3, 1, {makeUe(1, 1, periodicTraffic(100))});
  ran::SimulationEngine engine{std::move(config), std::move(scheduler), "recording"};

  static_cast<void>(engine.run());

  EXPECT_EQ(recorder->historicalThroughputs, std::vector<double>({0.0, 10.0, 19.0}));
}

TEST(SimulationEngineTest, RunsRoundRobinAcrossMultipleUes) {
  auto config = makeConfig(2, 1, {makeUe(1, 1, periodicTraffic()),
                                  makeUe(2, 1, periodicTraffic())});
  ran::SimulationEngine engine{std::move(config),
                               std::make_unique<ran::RoundRobinScheduler>(),
                               "round-robin"};

  const auto result = engine.run();

  EXPECT_EQ(result.totalTransmittedBytes, 200);
  EXPECT_EQ(result.perUe[0].transmittedBytes, 100);
  EXPECT_EQ(result.perUe[1].transmittedBytes, 100);
  EXPECT_DOUBLE_EQ(result.jainsFairnessIndex, 1.0);
}

TEST(SimulationEngineTest, RunsProportionalFairAcrossMultipleUes) {
  auto config = makeConfig(3, 1, {makeUe(1, 2, periodicTraffic()),
                                  makeUe(2, 1, periodicTraffic())});
  ran::SimulationEngine engine{std::move(config),
                               std::make_unique<ran::ProportionalFairScheduler>(),
                               "proportional-fair"};

  const auto result = engine.run();

  EXPECT_EQ(result.schedulerName, "proportional-fair");
  EXPECT_GT(result.perUe[0].transmittedBytes, 0);
  EXPECT_GT(result.perUe[1].transmittedBytes, 0);
  EXPECT_EQ(result.totalAllocatedResourceBlocks, 3);
}

TEST(SimulationEngineTest, IdenticalInputsProduceIdenticalResults) {
  const auto config = makeConfig(20, 3, {makeUe(1, 4, periodicTraffic(300, 5, 2)),
                                         makeUe(2, 8, periodicTraffic(400, 8, 3))},
                                 2026);
  ran::SimulationEngine first{config, std::make_unique<ran::RoundRobinScheduler>(),
                              "round-robin"};
  ran::SimulationEngine second{config, std::make_unique<ran::RoundRobinScheduler>(),
                               "round-robin"};

  const auto firstResult = first.run();
  const auto secondResult = second.run();

  EXPECT_EQ(firstResult.totalGeneratedBytes, secondResult.totalGeneratedBytes);
  EXPECT_EQ(firstResult.totalTransmittedBytes, secondResult.totalTransmittedBytes);
  EXPECT_EQ(firstResult.totalDroppedBytes, secondResult.totalDroppedBytes);
  ASSERT_EQ(firstResult.perSlot.size(), secondResult.perSlot.size());
  for (std::size_t index = 0; index < firstResult.perSlot.size(); ++index) {
    EXPECT_EQ(firstResult.perSlot[index].generatedBytes,
              secondResult.perSlot[index].generatedBytes);
    EXPECT_EQ(firstResult.perSlot[index].transmittedBytes,
              secondResult.perSlot[index].transmittedBytes);
    EXPECT_EQ(firstResult.perSlot[index].droppedBytes,
              secondResult.perSlot[index].droppedBytes);
    EXPECT_EQ(firstResult.perSlot[index].allocatedResourceBlocks,
              secondResult.perSlot[index].allocatedResourceBlocks);
  }
}

void expectInvalidAllocations(std::vector<ran::ResourceAllocation> allocations,
                              std::uint32_t resourceBlocks,
                              const std::vector<ran::UeConfig>& ues,
                              const std::string& expectedMessage) {
  auto config = makeConfig(1, resourceBlocks, ues);
  ran::SimulationEngine engine{
      std::move(config), std::make_unique<FixedScheduler>(std::move(allocations)), "invalid"};

  try {
    static_cast<void>(engine.run());
    FAIL() << "Expected invalid scheduler output to fail";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string{error.what()}.find(expectedMessage), std::string::npos)
        << error.what();
  }
}

TEST(SimulationEngineTest, RejectsInvalidSchedulerAllocations) {
  const std::vector<ran::UeConfig> oneUe{makeUe(1, 1, periodicTraffic())};
  const std::vector<ran::UeConfig> twoUes{makeUe(1, 1, periodicTraffic()),
                                          makeUe(2, 1, periodicTraffic())};

  expectInvalidAllocations({{99, 1}}, 1, oneUe, "unknown UE");
  expectInvalidAllocations({{1, 1}, {1, 1}}, 2, oneUe, "duplicate allocations");
  expectInvalidAllocations({{1, 0}}, 1, oneUe, "zero resource blocks");
  expectInvalidAllocations({{1, 1}, {2, 1}}, 1, twoUes, "slot capacity");
  expectInvalidAllocations({{1, 2}}, 2, oneUe, "useful queued demand");
}

}  // namespace
