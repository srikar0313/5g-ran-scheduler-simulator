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

int readIntegerValue(const Json& value, const std::string& path) {
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

int readInt(const Json& object, std::string_view name, const std::string& objectPath) {
  const auto& value = requireField(object, name, objectPath);
  return readIntegerValue(value, childPath(objectPath, name));
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
  const std::string emptyPath;
  const auto& simulation = requireObject(root, "simulation", emptyPath);
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
  if (model != "periodic" && model != "bernoulli" && model != "none") {
    configurationError(childPath(path, "model"),
                       "must be one of: periodic, bernoulli, none");
  }

  const auto packetSize = readUnsigned(traffic, "packet_size_bytes", path);
  if (packetSize == 0) {
    configurationError(childPath(path, "packet_size_bytes"), "must be greater than zero");
  }

  const auto latencyBudget = readUint32(traffic, "latency_budget_slots", path);
  if (latencyBudget == 0) {
    configurationError(childPath(path, "latency_budget_slots"), "must be greater than zero");
  }

  const auto categoryName = readString(traffic, "category", path);
  const auto category = parseTrafficCategory(categoryName, childPath(path, "category"));

  std::uint32_t period = 0;
  double arrivalProbability = 0.0;
  if (model == "periodic") {
    period = readUint32(traffic, "period_slots", path);
    if (period == 0) {
      configurationError(childPath(path, "period_slots"), "must be greater than zero");
    }
  } else if (model == "bernoulli") {
    arrivalProbability = readNumber(traffic, "arrival_probability", path);
    if (arrivalProbability < 0.0 || arrivalProbability > 1.0) {
      configurationError(childPath(path, "arrival_probability"),
                         "must be between 0 and 1");
    }
  }

  return TrafficConfig{model, packetSize, latencyBudget, period, arrivalProbability, category};
}

ChannelConfig parseChannel(const Json& ue, const std::string& uePath, int initialCqi) {
  const auto& channel = requireObject(ue, "channel", uePath);
  const auto path = childPath(uePath, "channel");
  const auto model = readString(channel, "model", path);
  if (model != "static" && model != "trace" && model != "random_walk") {
    configurationError(childPath(path, "model"),
                       "must be one of: static, trace, random_walk");
  }

  ChannelConfig config{model, {}, 1, 15};
  if (model == "trace") {
    const auto& trace = requireField(channel, "cqi_trace", path);
    const auto tracePath = childPath(path, "cqi_trace");
    if (!trace.is_array()) {
      configurationError(tracePath, "must be an array");
    }
    if (trace.empty()) {
      configurationError(tracePath, "must not be empty");
    }

    config.cqiTrace.reserve(trace.size());
    for (std::size_t index = 0; index < trace.size(); ++index) {
      const auto valuePath = tracePath + "[" + std::to_string(index) + "]";
      const auto cqi = readIntegerValue(trace.at(index), valuePath);
      if (cqi < 1 || cqi > 15) {
        configurationError(valuePath, "must be between 1 and 15");
      }
      config.cqiTrace.push_back(cqi);
    }
  } else if (model == "random_walk") {
    config.minCqi = readInt(channel, "min_cqi", path);
    config.maxCqi = readInt(channel, "max_cqi", path);
    if (config.minCqi < 1 || config.minCqi > 15) {
      configurationError(childPath(path, "min_cqi"), "must be between 1 and 15");
    }
    if (config.maxCqi < 1 || config.maxCqi > 15) {
      configurationError(childPath(path, "max_cqi"), "must be between 1 and 15");
    }
    if (config.minCqi > config.maxCqi) {
      configurationError(childPath(path, "min_cqi"), "must not exceed max_cqi");
    }
    if (initialCqi < config.minCqi || initialCqi > config.maxCqi) {
      configurationError(childPath(uePath, "initial_cqi"),
                         "must be inside the random-walk CQI range");
    }
  }

  return config;
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
                  parseChannel(value, path, initialCqi)};
}

SimulationConfig parseConfig(const Json& root) {
  if (!root.is_object()) {
    configurationError("root", "must be an object");
  }

  const std::string emptyPath;
  SimulationConfig config{parseSimulationSettings(root), {}};
  const auto& ues = requireField(root, "ues", emptyPath);
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
