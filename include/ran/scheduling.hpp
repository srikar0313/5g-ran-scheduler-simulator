#pragma once

#include <cstdint>

namespace ran {

struct UeSchedulingView {
  int ueId;
  std::uint64_t queuedBytes;
  int cqi;
};

struct ResourceAllocation {
  int ueId;
  std::uint32_t resourceBlocks;
};

}  // namespace ran
