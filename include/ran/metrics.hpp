#pragma once

#include "ran/simulation_results.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace ran {

double average(std::span<const std::uint32_t> values);
double nearestRankPercentile(std::vector<std::uint32_t> values, double percentile);
double jainsFairnessIndex(std::span<const PerUeMetrics> metrics);

}  // namespace ran
