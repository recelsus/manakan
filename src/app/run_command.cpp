#include "app.hpp"
#include "cli.hpp"
#include "config_loader.hpp"
#include "request_resolver.hpp"
#include "transport.hpp"
#include <iostream>
#include <string>
#include <unistd.h>

namespace manakan {
namespace {

void print_missing_config_guidance(const ConfigPaths& paths) {
  std::cerr << "No configuration files were found." << std::endl;
  std::cerr << "Create TOML files under:" << std::endl;
  std::cerr << "  providers: " << paths.providers_dir << std::endl;
  std::cerr << "  targets: " << paths.targets_dir << std::endl;
  std::cerr << "Examples are available in the repository under config/." << std::endl;
}

std::size_t count_total_targets(const LoadedConfig& config) {
  std::size_t total_targets = 0;
  for (const auto& kv : config.targets_by_name) total_targets += kv.second.size();
  return total_targets;
}

bool prepare_positional_input(CliOptions& cli) {
  if (!cli.positional_args.empty() || isatty(STDIN_FILENO)) return true;

  const std::string stdin_message = read_message_from_stdin();
  if (!stdin_message.empty()) cli.positional_args.push_back(stdin_message);
  return true;
}

LoadedConfig load_runtime_config(const ConfigPaths& paths) {
  try {
    return load_config_tree(paths);
  } catch (const std::runtime_error& ex) {
    const std::string message = ex.what();
    if (message.rfind("no providers found", 0) == 0 || message.rfind("no targets found", 0) == 0) {
      print_missing_config_guidance(paths);
    }
    throw;
  }
}

bool validate_target_selection(const ConfigPaths& paths, const LoadedConfig& config, const CliOptions& cli) {
  if (cli.target || config.defaults.default_target || count_total_targets(config) == 1) return true;

  std::cerr << "No target was specified and no default_target is configured." << std::endl;
  std::cerr << "Set default_target in " << paths.config_file << " or pass --target." << std::endl;
  return false;
}

void print_response(const ResolvedRequest& request, const HttpResponse& response) {
  std::cout << "Provider: " << request.provider_name << std::endl;
  std::cout << "Target: " << request.target_name << std::endl;
  std::cout << "Status: " << response.status << std::endl;
  if (!response.body.empty()) std::cout << "Response: " << response.body << std::endl;
}

} // namespace

int run_command(const std::string& app_name, CliOptions cli) {
  prepare_positional_input(cli);

  const auto paths = get_config_paths(app_name);
  if (!prepare_runtime_environment(paths)) return 1;

  const auto config = load_runtime_config(paths);
  if (!validate_target_selection(paths, config, cli)) return 1;

  const auto request = resolve_request(config, cli);
  const auto response = send_request(request);
  print_response(request, response);
  return 0;
}

} // namespace manakan
