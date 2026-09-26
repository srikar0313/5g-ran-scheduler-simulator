#include "ran/project_info.hpp"

#include <iostream>
#include <string_view>

namespace {

void printHelp(std::string_view executableName) {
  std::cout << executableName << " - " << ran::projectName() << '\n'
            << '\n'
            << "A simplified educational 5G RAN scheduling simulator.\n"
            << "Simulation features will be added incrementally in later phases.\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc > 1 && std::string_view{argv[1]} == "--help") {
    printHelp(argv[0]);
    return 0;
  }

  std::cout << ran::projectName() << '\n';
  return 0;
}
