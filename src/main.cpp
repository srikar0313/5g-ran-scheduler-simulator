#include "ran/config_loader.hpp"
#include "ran/project_info.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace {

void printHelp(std::string_view executableName) {
  std::cout << executableName << " - " << ran::projectName() << '\n'
            << '\n'
            << "A simplified educational 5G RAN scheduling simulator.\n"
            << "Scheduling and simulation are not implemented yet.\n"
            << '\n'
            << "Usage:\n"
            << "  " << executableName << " --config <path>\n"
            << "  " << executableName << " --help\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc == 1) {
    std::cout << ran::projectName() << '\n' << "Use --help to see available options.\n";
    return 0;
  }

  if (argc == 2 && std::string_view{argv[1]} == "--help") {
    printHelp(argv[0]);
    return 0;
  }

  std::optional<std::filesystem::path> configPath;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument != "--config") {
      std::cerr << "Error: unknown command-line option '" << argument << "'\n";
      return 1;
    }
    if (configPath.has_value()) {
      std::cerr << "Error: --config may only be specified once\n";
      return 1;
    }
    if (index + 1 >= argc || std::string_view{argv[index + 1]}.starts_with("--")) {
      std::cerr << "Error: missing value for --config\n";
      return 1;
    }
    configPath = argv[++index];
  }

  try {
    const auto config = ran::ConfigLoader::loadFromFile(*configPath);
    std::cout << "Configuration loaded successfully\n"
              << "UEs: " << config.ues.size() << '\n'
              << "Time slots: " << config.simulation.timeSlots << '\n'
              << "Resource blocks per slot: " << config.simulation.resourceBlocksPerSlot << '\n';
    return 0;
  } catch (const std::runtime_error& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
