#include "ran/config_loader.hpp"
#include "ran/project_info.hpp"
#include "ran/proportional_fair_scheduler.hpp"
#include "ran/result_writer.hpp"
#include "ran/round_robin_scheduler.hpp"
#include "ran/simulation_engine.hpp"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

struct CommandLineOptions {
  std::filesystem::path configPath;
  std::string schedulerName{"round-robin"};
  std::filesystem::path outputDirectory{"results"};
};

void printHelp(std::string_view executableName) {
  std::cout << executableName << " - " << ran::projectName() << '\n'
            << '\n'
            << "A simplified educational 5G RAN scheduling simulator.\n"
            << '\n'
            << "Usage:\n"
            << "  " << executableName
            << " --config <path> [--scheduler <name>] [--output-dir <path>]\n"
            << "  " << executableName << " --help\n"
            << '\n'
            << "Schedulers: round-robin, proportional-fair\n";
}

std::string_view requireValue(int argc, char* argv[], int& index,
                              std::string_view option) {
  if (index + 1 >= argc || std::string_view{argv[index + 1]}.starts_with("--")) {
    throw std::invalid_argument("missing value for " + std::string{option});
  }
  return argv[++index];
}

CommandLineOptions parseCommandLine(int argc, char* argv[]) {
  CommandLineOptions options;
  std::optional<std::filesystem::path> configPath;
  bool schedulerSpecified = false;
  bool outputDirectorySpecified = false;

  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--config") {
      if (configPath.has_value()) {
        throw std::invalid_argument("--config may only be specified once");
      }
      configPath = requireValue(argc, argv, index, argument);
    } else if (argument == "--scheduler") {
      if (schedulerSpecified) {
        throw std::invalid_argument("--scheduler may only be specified once");
      }
      options.schedulerName = requireValue(argc, argv, index, argument);
      schedulerSpecified = true;
    } else if (argument == "--output-dir") {
      if (outputDirectorySpecified) {
        throw std::invalid_argument("--output-dir may only be specified once");
      }
      options.outputDirectory = requireValue(argc, argv, index, argument);
      outputDirectorySpecified = true;
    } else {
      throw std::invalid_argument("unknown command-line option '" +
                                  std::string{argument} + "'");
    }
  }

  if (!configPath.has_value()) {
    throw std::invalid_argument("--config is required");
  }
  if (options.schedulerName != "round-robin" &&
      options.schedulerName != "proportional-fair") {
    throw std::invalid_argument("unsupported scheduler '" + options.schedulerName + "'");
  }
  options.configPath = std::move(*configPath);
  return options;
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc == 2 && std::string_view{argv[1]} == "--help") {
    printHelp(argv[0]);
    return 0;
  }

  try {
    const auto options = parseCommandLine(argc, argv);
    auto config = ran::ConfigLoader::loadFromFile(options.configPath);

    std::unique_ptr<ran::IScheduler> scheduler;
    if (options.schedulerName == "round-robin") {
      scheduler = std::make_unique<ran::RoundRobinScheduler>();
    } else {
      scheduler = std::make_unique<ran::ProportionalFairScheduler>();
    }

    ran::SimulationEngine engine{std::move(config), std::move(scheduler),
                                 options.schedulerName};
    const auto result = engine.run();
    ran::ResultWriter::write(result, options.outputDirectory);

    std::cout << std::fixed << std::setprecision(3)
              << "Simulation completed successfully\n"
              << "Scheduler: " << result.schedulerName << '\n'
              << "Transmitted bytes: " << result.totalTransmittedBytes << '\n'
              << "Dropped bytes: " << result.totalDroppedBytes << '\n'
              << "Resource-block utilization: " << result.resourceBlockUtilization << '\n'
              << "Jain's fairness index: " << result.jainsFairnessIndex << '\n'
              << "Output directory: " << options.outputDirectory.string() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << '\n';
    return 1;
  }
}
