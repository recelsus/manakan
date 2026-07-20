#include "diagnostics.hpp"
#include "merge.hpp"
#include <iostream>

namespace manakan {

int run_config_check(const ConfigPaths& paths, const LoadedConfig& config) {
  std::cout << "Config root: " << paths.root_dir.string() << std::endl;

  std::cout << "Providers (" << config.providers.size() << "):" << std::endl;
  for (const auto& kv : config.providers) {
    std::cout << "  - " << kv.first << "  (" << kv.second.source_file << ")" << std::endl;
  }

  std::cout << "Targets (" << config.targets.size() << "):" << std::endl;
  for (const auto& target : config.targets) {
    std::cout << "  - " << target.use << "." << target.name << (target.is_default ? " [default]" : "") << "  ("
               << target.source_file << ")" << std::endl;
  }

  int problems = 0;

  for (const auto& warning : config.warnings) {
    std::cerr << "warning: " << warning << std::endl;
    ++problems;
  }

  // Merging every target against its provider (without resolving placeholders,
  // which would require runtime input) is enough to surface every const
  // violation and structural conflict across the whole config tree, not just
  // whichever target a `send` happens to select.
  for (const auto& target : config.targets) {
    auto provider_it = config.providers.find(target.use);
    if (provider_it == config.providers.end()) continue; // already reported as a warning during load

    try {
      (void)merge_provider_and_target(provider_it->second, target);
    } catch (const std::exception& ex) {
      std::cerr << "error: " << target.use << "." << target.name << ": " << ex.what() << std::endl;
      ++problems;
    }
  }

  if (problems == 0) {
    std::cout << "OK: no problems found." << std::endl;
    return 0;
  }
  std::cout << problems << " problem(s) found." << std::endl;
  return 1;
}

} // namespace manakan
