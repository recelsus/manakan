#include "cli.hpp"
#include "config_loader.hpp"
#include "helpers.hpp"
#include "request_resolver.hpp"
#include "transport.hpp"
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {
constexpr const char* APP_NAME = "manakan";

void print_usage(const std::string& app_name) {
  std::cout << "Usage: " << app_name << " [--provider|-p name] [--target|-t name] [--input|-i key=value ...] [argv...]" << std::endl;
  std::cout << "Positional arguments are referenced from TOML as {{argv.1}}, {{argv.2}}, ..." << std::endl;
  std::cout << "Options: --help|-h --version|-v" << std::endl;
}

void print_version(const std::string& app_name) {
  std::cout << app_name << " " << MANAKAN_GIT_REV << std::endl;
}

bool is_yes_answer(const std::string& input) {
  return input == "y" || input == "Y" || input == "yes" || input == "YES";
}

bool ensure_config_directories(const manakan::ConfigPaths& paths) {
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

void print_missing_config_guidance(const manakan::ConfigPaths& paths) {
  std::cerr << "No configuration files were found." << std::endl;
  std::cerr << "Create TOML files under:" << std::endl;
  std::cerr << "  providers: " << paths.providers_dir << std::endl;
  std::cerr << "  targets: " << paths.targets_dir << std::endl;
  std::cerr << "Examples are available in the repository under config/." << std::endl;
}
} // namespace

int main(int argc, char* argv[]) {
  using namespace manakan;

  try {
    auto cli = parse_cli_options(argc, argv);
    if (cli.show_help) {
      print_usage(APP_NAME);
      return 0;
    }
    if (cli.show_version) {
      print_version(APP_NAME);
      return 0;
    }

    if (cli.positional_args.empty() && !isatty(STDIN_FILENO)) {
      const std::string stdin_message = read_message_from_stdin();
      if (!stdin_message.empty()) cli.positional_args.push_back(stdin_message);
    }

    const auto paths = get_config_paths(APP_NAME);
    if (!ensure_config_directories(paths)) return 1;
    LoadedConfig config;
    try {
      config = load_config_tree(paths);
    } catch (const std::runtime_error& ex) {
      const std::string message = ex.what();
      if (message.rfind("no providers found", 0) == 0 || message.rfind("no targets found", 0) == 0) {
        print_missing_config_guidance(paths);
      }
      throw;
    }

    std::size_t total_targets = 0;
    for (const auto& kv : config.targets_by_name) total_targets += kv.second.size();

    if (!cli.target && !config.defaults.default_target && total_targets != 1) {
      std::cerr << "No target was specified and no default_target is configured." << std::endl;
      std::cerr << "Set default_target in " << paths.config_file << " or pass --target." << std::endl;
      return 1;
    }

    const auto request = resolve_request(config, cli);
    const auto response = send_request(request);

    std::cout << "Provider: " << request.provider_name << std::endl;
    std::cout << "Target: " << request.target_name << std::endl;
    std::cout << "Status: " << response.status << std::endl;
    if (!response.body.empty()) std::cout << "Response: " << response.body << std::endl;
  } catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << std::endl;
    return 1;
  }
  return 0;
}
