#include "app.hpp"
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

namespace manakan {
namespace {

bool is_yes_answer(const std::string& input) {
  return input == "y" || input == "Y" || input == "yes" || input == "YES";
}

} // namespace

void print_usage(const std::string& app_name) {
  std::cout << "Usage: " << app_name
            << " [--provider|-p name] [--target|-t name] [--input|-i key=value ...] [argv...]"
            << std::endl;
  std::cout << "Positional arguments are referenced from TOML as {{argv.1}}, {{argv.2}}, ..."
            << std::endl;
  std::cout << "Options: --help|-h --version|-v" << std::endl;
}

void print_version(const std::string& app_name) { std::cout << app_name << " " << MANAKAN_GIT_REV << std::endl; }

bool prepare_runtime_environment(const ConfigPaths& paths) {
  namespace fs = std::filesystem;

  if (fs::exists(paths.providers_dir) && fs::exists(paths.targets_dir)) return true;

  std::cerr << "Configuration directories are missing." << std::endl;
  std::cerr << "  root: " << paths.root_dir << std::endl;
  std::cerr << "  providers: " << paths.providers_dir << std::endl;
  std::cerr << "  targets: " << paths.targets_dir << std::endl;

  if (!isatty(STDIN_FILENO)) {
    std::cerr << "Create these directories manually, then place your TOML files there." << std::endl;
    return false;
  }

  std::cout << "Create configuration directories now? [y/N]: ";
  std::string answer;
  if (!std::getline(std::cin, answer)) return false;
  if (!is_yes_answer(answer)) return false;

  fs::create_directories(paths.providers_dir);
  fs::create_directories(paths.targets_dir);
  std::cout << "Created: " << paths.providers_dir << std::endl;
  std::cout << "Created: " << paths.targets_dir << std::endl;
  std::cout << "Examples remain in the repository under config/." << std::endl;
  std::cout << "Create provider and target TOML files there, then run manakan again." << std::endl;
  std::cout << "Suggested files:" << std::endl;
  std::cout << "  - " << (paths.providers_dir / "discord.toml") << std::endl;
  std::cout << "  - " << (paths.targets_dir / "discord.toml") << std::endl;
  std::cout << "  - " << paths.config_file << " (optional, for default_target/default_provider)" << std::endl;
  return false;
}

} // namespace manakan
