#include "app.hpp"
#include "cli.hpp"
#include "config_loader.hpp"
#include "diagnostics.hpp"
#include "json.hpp"
#include "request_builder.hpp"
#include "request_resolver.hpp"
#include "transport.hpp"
#include <iostream>
#include <sstream>
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

void print_config_warnings(const LoadedConfig& config) {
  for (const auto& warning : config.warnings) std::cerr << "warning: " << warning << std::endl;
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

void print_response(const ResolvedRequest& request, const HttpResponse& response) {
  std::cout << "Provider: " << request.provider_name << std::endl;
  std::cout << "Target: " << request.target_name << std::endl;
  std::cout << "Status: " << response.status << std::endl;
  if (!response.body.empty()) std::cout << "Response: " << response.body << std::endl;
}

std::string shell_quote(const std::string& value) {
  std::string out = "'";
  for (char c : value) {
    if (c == '\'') out += "'\\''";
    else out += c;
  }
  out += "'";
  return out;
}

void print_config_summary(const ResolvedRequest& request, const BuiltHttpRequest& built) {
  std::cout << "Provider: " << request.provider_name << std::endl;
  std::cout << "Target: " << request.target_name << std::endl;
  std::cout << "Method: " << built.method << std::endl;
  std::cout << "URL: " << built.base_url << built.path << std::endl;
  std::cout << "Content-Type: " << built.content_type << std::endl;
  for (const auto& kv : built.headers) std::cout << "Header: " << kv.first << ": " << kv.second << std::endl;
  if (!built.body.empty()) std::cout << "Body: " << built.body << std::endl;
}

void print_config_json(const ResolvedRequest& request, const BuiltHttpRequest& built) {
  nlohmann::json out;
  out["provider"] = request.provider_name;
  out["target"] = request.target_name;
  out["method"] = built.method;
  out["url"] = built.base_url + built.path;
  out["headers"] = built.headers;
  out["content_type"] = built.content_type;
  if (built.content_type == "application/json" && !built.body.empty()) {
    out["body"] = nlohmann::json::parse(built.body, nullptr, false);
  } else {
    out["body"] = built.body;
  }
  std::cout << out.dump(2) << std::endl;
}

void print_config_curl(const BuiltHttpRequest& built) {
  std::ostringstream oss;
  oss << "curl -X " << built.method;
  for (const auto& kv : built.headers) oss << " -H " << shell_quote(kv.first + ": " + kv.second);
  if (!built.body.empty()) oss << " --data " << shell_quote(built.body);
  oss << " " << shell_quote(built.base_url + built.path);
  std::cout << oss.str() << std::endl;
}

int run_config_command(const ConfigPaths& paths, const LoadedConfig& config, CliOptions& cli) {
  if (cli.config_check) return run_config_check(paths, config);

  const auto request = resolve_request(config, cli);
  const auto built = build_http_request(request);

  if (cli.config_json) print_config_json(request, built);
  else if (cli.config_body) std::cout << built.body << std::endl;
  else if (cli.config_curl) print_config_curl(built);
  else print_config_summary(request, built);

  return 0;
}

int run_send_command(const LoadedConfig& config, CliOptions& cli) {
  const auto request = resolve_request(config, cli);
  const auto response = send_request(request);
  print_response(request, response);
  return 0;
}

} // namespace

int run_command(const std::string& app_name, CliOptions cli) {
  // `config --check` validates the whole config tree and never resolves a
  // target, so it must not block waiting for a message on stdin.
  if (!(cli.command == Command::Config && cli.config_check)) prepare_positional_input(cli);

  const auto paths = get_config_paths(app_name);
  if (!prepare_runtime_environment(paths)) return 1;

  const auto config = load_runtime_config(paths);
  print_config_warnings(config);

  if (cli.command == Command::Config) return run_config_command(paths, config, cli);
  return run_send_command(config, cli);
}

} // namespace manakan
