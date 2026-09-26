#pragma once

#include <cstdint>

namespace ran {

struct UeSchedulingView {
  int ueId;
  std::uint64_t queuedBytes;
  int cqi;
  double historicalAverageThroughput{0.0};

  UeSchedulingView(int id, std::uint64_t bytes, int channelQuality,
                   double historicalThroughput = 0.0)
      : ueId(id),
        queuedBytes(bytes),
        cqi(channelQuality),
        historicalAverageThroughput(historicalThroughput) {}
};

struct ResourceAllocation {
  int ueId;
  std::uint32_t resourceBlocks;
};

}  // namespace ran
