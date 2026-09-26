#include "ran/result_writer.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace ran {
namespace {

using Json = nlohmann::json;

std::ofstream openOutputFile(const std::filesystem::path& path) {
  std::ofstream output{path};
  if (!output.is_open()) {
    throw std::runtime_error("Could not create result file '" + path.string() + "'");
  }
  return output;
}

void writeSummary(const SimulationResult& result,
                  const std::filesystem::path& outputDirectory) {
  Json summary;
  summary["scheduler_name"] = result.schedulerName;
  summary["simulation_seed"] = result.simulationSeed;
  summary["time_slots"] = result.timeSlots;
  summary["total_generated_bytes"] = result.totalGeneratedBytes;
  summary["total_transmitted_bytes"] = result.totalTransmittedBytes;
  summary["total_dropped_bytes"] = result.totalDroppedBytes;
  summary["total_allocated_resource_blocks"] = result.totalAllocatedResourceBlocks;
  summary["total_available_resource_blocks"] = result.totalAvailableResourceBlocks;
  summary["resource_block_utilization"] = result.resourceBlockUtilization;
  summary["jains_fairness_index"] = result.jainsFairnessIndex;
  summary["per_ue"] = Json::array();

  for (const auto& ue : result.perUe) {
    summary["per_ue"].push_back(
        {{"ue_id", ue.ueId},
         {"generated_packets", ue.generatedPackets},
         {"generated_bytes", ue.generatedBytes},
         {"completed_packets", ue.completedPackets},
         {"transmitted_bytes", ue.transmittedBytes},
         {"dropped_packets", ue.droppedPackets},
         {"dropped_bytes", ue.droppedBytes},
         {"allocated_resource_blocks", ue.allocatedResourceBlocks},
         {"average_throughput_bytes_per_slot", ue.averageThroughputBytesPerSlot},
         {"average_completed_packet_latency_slots",
          ue.averageCompletedPacketLatencySlots},
         {"percentile_95_completed_packet_latency_slots",
          ue.percentile95CompletedPacketLatencySlots}});
  }

  auto output = openOutputFile(outputDirectory / "summary.json");
  output << summary.dump(2) << '\n';
  if (!output) {
    throw std::runtime_error("Could not write summary.json");
  }
}

void writePerUe(const SimulationResult& result,
                const std::filesystem::path& outputDirectory) {
  auto output = openOutputFile(outputDirectory / "per_ue.csv");
  output << "ue_id,generated_packets,generated_bytes,completed_packets,"
            "transmitted_bytes,dropped_packets,dropped_bytes,allocated_resource_blocks,"
            "average_throughput_bytes_per_slot,"
            "average_completed_packet_latency_slots,"
            "percentile_95_completed_packet_latency_slots\n";
  output << std::setprecision(17);
  for (const auto& ue : result.perUe) {
    output << ue.ueId << ',' << ue.generatedPackets << ',' << ue.generatedBytes << ','
           << ue.completedPackets << ',' << ue.transmittedBytes << ','
           << ue.droppedPackets << ',' << ue.droppedBytes << ','
           << ue.allocatedResourceBlocks << ',' << ue.averageThroughputBytesPerSlot << ','
           << ue.averageCompletedPacketLatencySlots << ','
           << ue.percentile95CompletedPacketLatencySlots << '\n';
  }
  if (!output) {
    throw std::runtime_error("Could not write per_ue.csv");
  }
}

void writePerSlot(const SimulationResult& result,
                  const std::filesystem::path& outputDirectory) {
  auto output = openOutputFile(outputDirectory / "per_slot.csv");
  output << "slot,generated_bytes,transmitted_bytes,dropped_bytes,"
            "allocated_resource_blocks\n";
  for (const auto& slot : result.perSlot) {
    output << slot.slot << ',' << slot.generatedBytes << ',' << slot.transmittedBytes << ','
           << slot.droppedBytes << ',' << slot.allocatedResourceBlocks << '\n';
  }
  if (!output) {
    throw std::runtime_error("Could not write per_slot.csv");
  }
}

}  // namespace

void ResultWriter::write(const SimulationResult& result,
                         const std::filesystem::path& outputDirectory) {
  std::filesystem::create_directories(outputDirectory);
  writeSummary(result, outputDirectory);
  writePerUe(result, outputDirectory);
  writePerSlot(result, outputDirectory);
}

}  // namespace ran
