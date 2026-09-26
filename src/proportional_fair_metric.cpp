#include "ran/proportional_fair_metric.hpp"

#include "ran/cqi_capacity.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ran {

double proportionalFairMetric(const UeSchedulingView& ue, double epsilon) {
  if (!std::isfinite(epsilon) || epsilon <= 0.0) {
    throw std::invalid_argument("PF epsilon must be finite and greater than zero");
  }
  if (!std::isfinite(ue.historicalAverageThroughput) ||
      ue.historicalAverageThroughput < 0.0) {
    throw std::invalid_argument("Historical throughput must be finite and not negative");
  }

  const auto estimatedRate = static_cast<double>(bytesPerResourceBlock(ue.cqi));
  const auto denominator = std::max(ue.historicalAverageThroughput, epsilon);
  return estimatedRate / denominator;
}

}  // namespace ran
