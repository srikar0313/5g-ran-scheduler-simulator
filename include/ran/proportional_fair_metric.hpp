#pragma once

#include "ran/scheduling.hpp"

namespace ran {

double proportionalFairMetric(const UeSchedulingView& ue, double epsilon = 1.0);

}  // namespace ran
