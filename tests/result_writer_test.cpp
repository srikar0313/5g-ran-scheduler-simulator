#include "ran/result_writer.hpp"

#include "ran/round_robin_scheduler.hpp"
#include "ran/simulation_engine.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

namespace {

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    static std::atomic<std::uint64_t> counter{0};
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("ran_results_test_" + std::to_string(timestamp) + "_" +
             std::to_string(counter.fetch_add(1)));
  }

  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  TemporaryDirectory(const TemporaryDirectory&) = delete;
  TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

std::string readFile(const std::filesystem::path& path) {
  std::ifstream input{path};
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

ran::SimulationResult sampleResult() {
  ran::SimulationResult result;
  result.schedulerName = "round-robin";
  result.simulationSeed = 42;
  result.timeSlots = 2;
  result.totalGeneratedBytes = 200;
  result.totalTransmittedBytes = 150;
  result.totalDroppedBytes = 25;
  result.totalAllocatedResourceBlocks = 2;
  result.totalAvailableResourceBlocks = 4;
  result.resourceBlockUtilization = 0.5;
  result.jainsFairnessIndex = 1.0;

  ran::PerUeMetrics ue;
  ue.ueId = 7;
  ue.generatedPackets = 2;
  ue.generatedBytes = 200;
  ue.completedPackets = 1;
  ue.transmittedBytes = 150;
  ue.droppedPackets = 1;
  ue.droppedBytes = 25;
  ue.allocatedResourceBlocks = 2;
  ue.averageThroughputBytesPerSlot = 75.0;
  ue.averageCompletedPacketLatencySlots = 2.0;
  ue.percentile95CompletedPacketLatencySlots = 2.0;
  result.perUe.push_back(ue);
  result.perSlot.push_back({0, 100, 100, 0, 1});
  result.perSlot.push_back({1, 100, 50, 25, 1});
  return result;
}

ran::SimulationConfig deterministicConfig() {
  const ran::TrafficConfig traffic{"bernoulli", 200, 5, 0, 0.5,
                                   ran::TrafficCategory::Video};
  const ran::ChannelConfig channel{"random_walk", {}, 3, 12};
  const ran::UeConfig ue{1, 1, 8, traffic, channel};
  return {{20, 1.0, 2, 2026}, {ue}};
}

TEST(ResultWriterTest, CreatesJsonAndCsvFiles) {
  const TemporaryDirectory directory;

  ran::ResultWriter::write(sampleResult(), directory.path());

  const auto summaryPath = directory.path() / "summary.json";
  const auto perUePath = directory.path() / "per_ue.csv";
  const auto perSlotPath = directory.path() / "per_slot.csv";
  EXPECT_TRUE(std::filesystem::is_regular_file(summaryPath));
  EXPECT_TRUE(std::filesystem::is_regular_file(perUePath));
  EXPECT_TRUE(std::filesystem::is_regular_file(perSlotPath));

  const auto summary = nlohmann::json::parse(readFile(summaryPath));
  EXPECT_EQ(summary.at("scheduler_name"), "round-robin");
  EXPECT_EQ(summary.at("total_transmitted_bytes"), 150);
  ASSERT_EQ(summary.at("per_ue").size(), 1);
  EXPECT_EQ(summary.at("per_ue")[0].at("ue_id"), 7);

  const auto perUe = readFile(perUePath);
  EXPECT_NE(perUe.find("ue_id,generated_packets,generated_bytes"), std::string::npos);
  EXPECT_NE(perUe.find("7,2,200,1,150,1,25,2,75,2,2"), std::string::npos);
  const auto perSlot = readFile(perSlotPath);
  EXPECT_NE(perSlot.find("slot,generated_bytes,transmitted_bytes"), std::string::npos);
  EXPECT_NE(perSlot.find("0,100,100,0,1"), std::string::npos);
}

TEST(ResultWriterTest, DeterministicRunsCreateIdenticalFiles) {
  const TemporaryDirectory firstDirectory;
  const TemporaryDirectory secondDirectory;
  ran::SimulationEngine first{deterministicConfig(),
                              std::make_unique<ran::RoundRobinScheduler>(),
                              "round-robin"};
  ran::SimulationEngine second{deterministicConfig(),
                               std::make_unique<ran::RoundRobinScheduler>(),
                               "round-robin"};

  ran::ResultWriter::write(first.run(), firstDirectory.path());
  ran::ResultWriter::write(second.run(), secondDirectory.path());

  for (const auto* filename : {"summary.json", "per_ue.csv", "per_slot.csv"}) {
    EXPECT_EQ(readFile(firstDirectory.path() / filename),
              readFile(secondDirectory.path() / filename));
  }
}

}  // namespace
