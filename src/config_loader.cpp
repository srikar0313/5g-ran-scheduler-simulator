#include "ran/config_loader.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace ran {
namespace {

using Json = nlohmann::json;

[[noreturn]] void configurationError(const std::string& field, const std::string& message) {
  throw std::runtime_error("Configuration error: " + field + " " + message);
}

std::string childPath(const std::string& parent, std::string_view child) {
  if (parent.empty()) {
    return std::string{child};
  }
  return parent + "." + std::string{child};
}

const Json& requireField(const Json& object, std::string_view name,
                         const std::string& objectPath) {
  const auto path = childPath(objectPath, name);
  const auto key = std::string{name};
  if (!object.contains(key)) {
    configurationError(path, "is required");
  }
  return object.at(key);
}

const Json& requireObject(const Json& object, std::string_view name,
                          const std::string& objectPath) {
  const auto& value = requireField(object, name, objectPath);
  const auto path = childPath(objectPath, name);
  if (!value.is_object()) {
    configurationError(path, "must be an object");
  }
  return value;
}

std::uint64_t readUnsigned(const Json& object, std::string_view name,
                           const std::string& objectPath) {
  const auto& value = requireField(object, name, objectPath);
  const auto path = childPath(objectPath, name);
  if (!value.is_number_unsigned()) {
    configurationError(path, "must be an unsigned integer");
  }
  return value.get<std::uint64_t>();
}

std::uint32_t readUint32(const Json& object, std::string_view name,
                         const std::string& objectPath) {
  const auto path = childPath(objectPath, name);
  const auto value = readUnsigned(object, name, objectPath);
  if (value > std::numeric_limits<std::uint32_t>::max()) {
    configurationError(path, "is too large");
  }
  return static_cast<std::uint32_t>(value);
}

int readInt(const Json& object, std::string_view name, const std::string& objectPath) {
  const auto& value = requireField(object, name, objectPath);
  const auto path = childPath(objectPath, name);

  if (value.is_number_unsigned()) {
    const auto number = value.get<std::uint64_t>();
    if (number > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
      configurationError(path, "is outside the supported integer range");
    }
    return static_cast<int>(number);
  }

  if (value.is_number_integer()) {
    const auto number = value.get<std::int64_t>();
    if (number < std::numeric_limits<int>::min() ||
        number > std::numeric_limits<int>::max()) {
      configurationError(path, "is outside the supported integer range");
    }
    return static_cast<int>(number);
  }

  configurationError(path, "must be an integer");
}

double readNumber(const Json& object, std::string_view name, const std::string& objectPath) {
  const auto& value = requireField(object, name, objectPath);
  const auto path = childPath(objectPath, name);
  if (!value.is_number()) {
    configurationError(path, "must be a number");
  }

  const auto number = value.get<double>();
  if (!std::isfinite(number)) {
    configurationError(path, "must be finite");
  }
  return number;
}

std::string readString(const Json& object, std::string_view name,
                       const std::string& objectPath) {
  const auto& value = requireField(object, name, objectPath);
  const auto path = childPath(objectPath, name);
  if (!value.is_string()) {
    configurationError(path, "must be a string");
  }
  return value.get<std::string>();
}

TrafficCategory parseTrafficCategory(const std::string& category, const std::string& path) {
  if (category == "voice") {
    return TrafficCategory::Voice;
  }
  if (category == "video") {
    return TrafficCategory::Video;
  }
  if (category == "download") {
    return TrafficCategory::Download;
  }
  configurationError(path, "must be one of: voice, video, download");
}

SimulationSettings parseSimulationSettings(const Json& root) {
  const auto& simulation = requireObject(root, "simulation", "");
  const std::string path = "simulation";

  const auto timeSlots = readUint32(simulation, "time_slots", path);
  if (timeSlots == 0) {
    configurationError("simulation.time_slots", "must be greater than zero");
  }

  const auto slotDurationMs = readNumber(simulation, "slot_duration_ms", path);
  if (slotDurationMs <= 0.0) {
    configurationError("simulation.slot_duration_ms", "must be greater than zero");
  }

  const auto resourceBlocks = readUint32(simulation, "resource_blocks_per_slot", path);
  if (resourceBlocks == 0) {
    configurationError("simulation.resource_blocks_per_slot", "must be greater than zero");
  }

  return SimulationSettings{timeSlots, slotDurationMs, resourceBlocks,
                            readUint32(simulation, "random_seed", path)};
}

TrafficConfig parseTraffic(const Json& ue, const std::string& uePath) {
  const auto& traffic = requireObject(ue, "traffic", uePath);
  const auto path = childPath(uePath, "traffic");

  const auto model = readString(traffic, "model", path);
  if (model != "periodic") {
    configurationError(childPath(path, "model"), "must be \"periodic\"");
  }

  const auto packetSize = readUnsigned(traffic, "packet_size_bytes", path);
  if (packetSize == 0) {
    configurationError(childPath(path, "packet_size_bytes"), "must be greater than zero");
  }

  const auto latencyBudget = readUint32(traffic, "latency_budget_slots", path);
  if (latencyBudget == 0) {
    configurationError(childPath(path, "latency_budget_slots"), "must be greater than zero");
  }

  const auto period = readUint32(traffic, "period_slots", path);
  if (period == 0) {
    configurationError(childPath(path, "period_slots"), "must be greater than zero");
  }

  const auto categoryName = readString(traffic, "category", path);
  const auto category = parseTrafficCategory(categoryName, childPath(path, "category"));

  return TrafficConfig{model, packetSize, latencyBudget, period, category};
}

ChannelConfig parseChannel(const Json& ue, const std::string& uePath) {
  const auto& channel = requireObject(ue, "channel", uePath);
  const auto path = childPath(uePath, "channel");
  const auto model = readString(channel, "model", path);
  if (model != "static") {
    configurationError(childPath(path, "model"), "must be \"static\"");
  }
  return ChannelConfig{model};
}

UeConfig parseUe(const Json& value, std::size_t index) {
  const auto path = "ues[" + std::to_string(index) + "]";
  if (!value.is_object()) {
    configurationError(path, "must be an object");
  }

  const auto priority = readInt(value, "priority", path);
  if (priority <= 0) {
    configurationError(childPath(path, "priority"), "must be positive");
  }

  const auto initialCqi = readInt(value, "initial_cqi", path);
  if (initialCqi < 1 || initialCqi > 15) {
    configurationError(childPath(path, "initial_cqi"), "must be between 1 and 15");
  }

  return UeConfig{readInt(value, "id", path), priority, initialCqi, parseTraffic(value, path),
                  parseChannel(value, path)};
}

SimulationConfig parseConfig(const Json& root) {
  if (!root.is_object()) {
    configurationError("root", "must be an object");
  }

  SimulationConfig config{parseSimulationSettings(root), {}};
  const auto& ues = requireField(root, "ues", "");
  if (!ues.is_array()) {
    configurationError("ues", "must be an array");
  }
  if (ues.empty()) {
    configurationError("ues", "must contain at least one UE");
  }

  std::unordered_set<int> ueIds;
  config.ues.reserve(ues.size());
  for (std::size_t index = 0; index < ues.size(); ++index) {
    auto ue = parseUe(ues.at(index), index);
    if (!ueIds.insert(ue.id).second) {
      configurationError("ues[" + std::to_string(index) + "].id", "must be unique");
    }
    config.ues.push_back(std::move(ue));
  }

  return config;
}

}  // namespace

SimulationConfig ConfigLoader::loadFromFile(const std::filesystem::path& path) {
  std::ifstream input{path};
  if (!input.is_open()) {
    throw std::runtime_error("Configuration error: cannot open file '" + path.string() + "'");
  }

  Json root;
  try {
    input >> root;
  } catch (const Json::exception& error) {
    throw std::runtime_error("Configuration error: invalid JSON in '" + path.string() +
                             "': " + error.what());
  }

  try {
    return parseConfig(root);
  } catch (const std::runtime_error&) {
    throw;
  } catch (const Json::exception& error) {
    throw std::runtime_error("Configuration error: " + std::string{error.what()});
  }
}

}  // namespace ran
