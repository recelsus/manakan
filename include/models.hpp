#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace manakan {

struct CliOptions {
  std::optional<std::string> provider;
  std::optional<std::string> target;
  std::unordered_map<std::string, std::string> inputs;
  std::vector<std::string> positional_args;
  bool show_help = false;
  bool show_version = false;
};

struct ConfigPaths {
  std::filesystem::path root_dir;
  std::filesystem::path providers_dir;
  std::filesystem::path targets_dir;
  std::filesystem::path config_file;
};

struct AppDefaults {
  std::optional<std::string> default_provider;
  std::optional<std::string> default_target;
};

struct ProviderConfig {
  std::string name;
  std::string method;
  std::string base_url;
  std::string path;
  std::unordered_map<std::string, std::string> values;
  std::unordered_map<std::string, std::string> headers;
  std::unordered_map<std::string, std::string> body;
};

struct TargetConfig {
  std::string name;
  std::string use;
  std::unordered_map<std::string, std::string> values;
};

struct LoadedConfig {
  AppDefaults defaults;
  std::unordered_map<std::string, ProviderConfig> providers;
  std::unordered_map<std::string, std::vector<TargetConfig>> targets_by_name;
};

struct ResolvedRequest {
  std::string provider_name;
  std::string target_name;
  std::string method;
  std::string base_url;
  std::string path;
  std::unordered_map<std::string, std::string> headers;
  std::unordered_map<std::string, std::string> body;
};

struct HttpResponse {
  int status = 0;
  std::string body;
};

} // namespace manakan
