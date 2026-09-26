#include "ran/config_loader.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using Json = nlohmann::json;

std::filesystem::path makeTemporaryPath() {
  static std::atomic<std::uint64_t> counter{0};
  const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("ran_config_test_" + std::to_string(timestamp) + "_" +
          std::to_string(counter.fetch_add(1)) + ".json");
}

class TemporaryJsonFile {
 public:
  explicit TemporaryJsonFile(const std::string& contents) : path_(makeTemporaryPath()) {
    std::ofstream output{path_};
    if (!output.is_open()) {
      throw std::runtime_error("Could not create temporary configuration file");
    }
    output << contents;
  }

  ~TemporaryJsonFile() {
    std::error_code error;
    std::filesystem::remove(path_, error);
  }

  TemporaryJsonFile(const TemporaryJsonFile&) = delete;
  TemporaryJsonFile& operator=(const TemporaryJsonFile&) = delete;

  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

Json validConfig() {
  return {
      {"simulation",
       {{"time_slots", 100},
        {"slot_duration_ms", 1.0},
        {"resource_blocks_per_slot", 10},
        {"random_seed", 42}}},
      {"ues",
       Json::array({{{"id", 1},
                     {"priority", 2},
                     {"initial_cqi", 10},
                     {"traffic",
                      {{"model", "periodic"},
                       {"packet_size_bytes", 500},
                       {"latency_budget_slots", 5},
                       {"period_slots", 2},
                       {"category", "video"}}},
                     {"channel", {{"model", "static"}}}}})}};
}

void expectConfigError(const Json& config, std::string_view expectedText) {
  const TemporaryJsonFile file{config.dump()};
  try {
    static_cast<void>(ran::ConfigLoader::loadFromFile(file.path()));
    FAIL() << "Expected configuration loading to fail";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string{error.what()}.find(expectedText), std::string::npos)
        << "Actual message: " << error.what();
  }
}

TEST(ConfigLoaderTest, LoadsValidConfiguration) {
  const TemporaryJsonFile file{validConfig().dump()};

  const auto config = ran::ConfigLoader::loadFromFile(file.path());

  ASSERT_EQ(config.ues.size(), 1);
  EXPECT_EQ(config.simulation.timeSlots, 100);
  EXPECT_DOUBLE_EQ(config.simulation.slotDurationMs, 1.0);
  EXPECT_EQ(config.simulation.resourceBlocksPerSlot, 10);
  EXPECT_EQ(config.ues.front().initialCqi, 10);
  EXPECT_EQ(config.ues.front().traffic.category, ran::TrafficCategory::Video);
}

TEST(ConfigLoaderTest, RejectsMissingFile) {
  const auto path = makeTemporaryPath();

  EXPECT_THROW(static_cast<void>(ran::ConfigLoader::loadFromFile(path)), std::runtime_error);
}

TEST(ConfigLoaderTest, RejectsInvalidJson) {
  const TemporaryJsonFile file{std::string{"{ invalid json"}};

  try {
    static_cast<void>(ran::ConfigLoader::loadFromFile(file.path()));
    FAIL() << "Expected invalid JSON to fail";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string{error.what()}.find("invalid JSON"), std::string::npos);
  }
}

TEST(ConfigLoaderTest, RejectsMissingRequiredField) {
  auto config = validConfig();
  config["simulation"].erase("time_slots");

  expectConfigError(config, "simulation.time_slots is required");
}

TEST(ConfigLoaderTest, RejectsZeroTimeSlots) {
  auto config = validConfig();
  config["simulation"]["time_slots"] = 0;

  expectConfigError(config, "simulation.time_slots must be greater than zero");
}

TEST(ConfigLoaderTest, RejectsZeroSlotDuration) {
  auto config = validConfig();
  config["simulation"]["slot_duration_ms"] = 0.0;

  expectConfigError(config, "simulation.slot_duration_ms must be greater than zero");
}

TEST(ConfigLoaderTest, RejectsZeroResourceBlocks) {
  auto config = validConfig();
  config["simulation"]["resource_blocks_per_slot"] = 0;

  expectConfigError(config, "simulation.resource_blocks_per_slot must be greater than zero");
}

TEST(ConfigLoaderTest, RejectsEmptyUeList) {
  auto config = validConfig();
  config["ues"] = Json::array();

  expectConfigError(config, "ues must contain at least one UE");
}

TEST(ConfigLoaderTest, RejectsDuplicateUeIdentifiers) {
  auto config = validConfig();
  config["ues"].push_back(config["ues"].front());

  expectConfigError(config, "ues[1].id must be unique");
}

TEST(ConfigLoaderTest, RejectsInvalidCqi) {
  auto config = validConfig();
  config["ues"][0]["initial_cqi"] = 16;

  expectConfigError(config, "ues[0].initial_cqi must be between 1 and 15");
}

TEST(ConfigLoaderTest, RejectsInvalidPriority) {
  auto config = validConfig();
  config["ues"][0]["priority"] = 0;

  expectConfigError(config, "ues[0].priority must be positive");
}

TEST(ConfigLoaderTest, RejectsZeroPacketSize) {
  auto config = validConfig();
  config["ues"][0]["traffic"]["packet_size_bytes"] = 0;

  expectConfigError(config, "ues[0].traffic.packet_size_bytes must be greater than zero");
}

TEST(ConfigLoaderTest, RejectsZeroLatencyBudget) {
  auto config = validConfig();
  config["ues"][0]["traffic"]["latency_budget_slots"] = 0;

  expectConfigError(config, "ues[0].traffic.latency_budget_slots must be greater than zero");
}

TEST(ConfigLoaderTest, RejectsZeroPeriod) {
  auto config = validConfig();
  config["ues"][0]["traffic"]["period_slots"] = 0;

  expectConfigError(config, "ues[0].traffic.period_slots must be greater than zero");
}

TEST(ConfigLoaderTest, RejectsUnknownTrafficModel) {
  auto config = validConfig();
  config["ues"][0]["traffic"]["model"] = "bursty";

  expectConfigError(config, "ues[0].traffic.model must be \"periodic\"");
}

TEST(ConfigLoaderTest, RejectsUnknownTrafficCategory) {
  auto config = validConfig();
  config["ues"][0]["traffic"]["category"] = "gaming";

  expectConfigError(config, "ues[0].traffic.category");
}

TEST(ConfigLoaderTest, RejectsUnknownChannelModel) {
  auto config = validConfig();
  config["ues"][0]["channel"]["model"] = "fading";

  expectConfigError(config, "ues[0].channel.model must be \"static\"");
}

}  // namespace
