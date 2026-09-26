#pragma once

#include "ran/simulation_results.hpp"

#include <filesystem>

namespace ran {

class ResultWriter {
 public:
  static void write(const SimulationResult& result,
                    const std::filesystem::path& outputDirectory);
};

}  // namespace ran
