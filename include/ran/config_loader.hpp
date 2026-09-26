#pragma once

#include "ran/simulation_config.hpp"

#include <filesystem>

namespace ran {

class ConfigLoader {
 public:
  static SimulationConfig loadFromFile(const std::filesystem::path& path);
};

}  // namespace ran
