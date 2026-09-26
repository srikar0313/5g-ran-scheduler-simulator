#include "ran/metrics.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ran {

double average(std::span<const std::uint32_t> values) {
  if (values.empty()) {
    return 0.0;
  }

  double sum = 0.0;
  for (const auto value : values) {
    sum += static_cast<double>(value);
  }
  return sum / static_cast<double>(values.size());
}

double nearestRankPercentile(std::vector<std::uint32_t> values, double percentile) {
  if (!std::isfinite(percentile) || percentile <= 0.0 || percentile > 1.0) {
    throw std::invalid_argument("Percentile must be finite and in the range (0, 1]");
  }
  if (values.empty()) {
    return 0.0;
  }

  std::sort(values.begin(), values.end());
  const auto rank = static_cast<std::size_t>(
      std::ceil(percentile * static_cast<double>(values.size())));
  return static_cast<double>(values[rank - 1]);
}

double jainsFairnessIndex(std::span<const PerUeMetrics> metrics) {
  if (metrics.empty()) {
    return 0.0;
  }

  double sum = 0.0;
  double sumOfSquares = 0.0;
  for (const auto& ue : metrics) {
    sum += ue.averageThroughputBytesPerSlot;
    sumOfSquares += ue.averageThroughputBytesPerSlot *
                    ue.averageThroughputBytesPerSlot;
  }
  if (sumOfSquares == 0.0) {
    return 0.0;
  }
  return (sum * sum) / (static_cast<double>(metrics.size()) * sumOfSquares);
}

}  // namespace ran
