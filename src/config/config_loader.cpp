#include "config_loader.hpp"
#include <algorithm>
#include <cstdlib>
#include <set>
#include <stdexcept>
#include <toml.hpp>

namespace manakan {
namespace {

constexpr const char* kProvidersDir = "providers";
constexpr const char* kTargetsDir = "targets";
constexpr const char* kConfigFile = "config.toml";

bool is_reserved_provider_key(const std::string& key) {
  static const std::set<std::string> reserved = {"name", "method", "base_url", "path", "headers", "body"};
  return reserved.find(key) != reserved.end();
}

std::string toml_node_to_string(const toml::node& node) {
  if (auto value = node.value<std::string>()) return *value;
  if (auto value = node.value<int64_t>()) return std::to_string(*value);
  if (auto value = node.value<double>()) return std::to_string(*value);
  if (auto value = node.value<bool>()) return *value ? "true" : "false";
  throw std::runtime_error("unsupported TOML value type");
}

std::unordered_map<std::string, std::string> parse_string_table(const toml::table* table, const std::string& name) {
  std::unordered_map<std::string, std::string> out;
  if (!table) return out;

  for (const auto& kv : *table) {
    out.emplace(kv.first.str(), toml_node_to_string(kv.second));
  }
  return out;
}

std::vector<std::filesystem::path> list_toml_files(const std::filesystem::path& dir) {
  std::vector<std::filesystem::path> files;
  if (!std::filesystem::exists(dir)) return files;

  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (!entry.is_regular_file()) continue;
    if (entry.path().extension() == ".toml") files.push_back(entry.path());
  }

  std::sort(files.begin(), files.end());
  return files;
}

AppDefaults load_defaults_file(const std::filesystem::path& path) {
  AppDefaults defaults;
  if (!std::filesystem::exists(path)) return defaults;

  auto cfg = toml::parse_file(path.string());
  if (auto value = cfg["default_provider"].value<std::string>()) defaults.default_provider = *value;
  if (auto value = cfg["default_target"].value<std::string>()) defaults.default_target = *value;
  return defaults;
}

void load_provider_files(const std::filesystem::path& dir, std::unordered_map<std::string, ProviderConfig>& providers) {
  for (const auto& path : list_toml_files(dir)) {
    auto cfg = toml::parse_file(path.string());

    ProviderConfig provider;
    provider.name = cfg["name"].value_or(std::string());
    provider.method = cfg["method"].value_or(std::string());
    provider.base_url = cfg["base_url"].value_or(std::string());
    provider.path = cfg["path"].value_or(std::string("/"));

    if (provider.name.empty()) throw std::runtime_error("provider file missing name: " + path.string());
    if (provider.method.empty()) throw std::runtime_error("provider file missing method: " + path.string());
    if (provider.base_url.empty()) throw std::runtime_error("provider file missing base_url: " + path.string());

    if (cfg["headers"] && !cfg["headers"].is_table()) throw std::runtime_error("headers must be a table: " + path.string());
    if (cfg["body"] && !cfg["body"].is_table()) throw std::runtime_error("body must be a table: " + path.string());
    provider.headers = parse_string_table(cfg["headers"].as_table(), "headers");
    provider.body = parse_string_table(cfg["body"].as_table(), "body");

    for (const auto& kv : cfg) {
      const std::string key(kv.first.str());
      if (is_reserved_provider_key(key)) continue;
      if (kv.second.is_table()) continue;
      provider.values.emplace(key, toml_node_to_string(kv.second));
    }

    if (!providers.emplace(provider.name, provider).second) {
      throw std::runtime_error("duplicate provider name: " + provider.name);
    }
  }
}

void load_target_files(const std::filesystem::path& dir, std::unordered_map<std::string, std::vector<TargetConfig>>& targets_by_name) {
  for (const auto& path : list_toml_files(dir)) {
    auto cfg = toml::parse_file(path.string());
    const auto use = cfg["use"].value_or(std::string());
    if (use.empty()) throw std::runtime_error("target file missing use: " + path.string());

    for (const auto& kv : cfg) {
      const std::string key(kv.first.str());
      if (key == "use") continue;
      if (!kv.second.is_table()) continue;

      TargetConfig target;
      target.name = key;
      target.use = use;

      const auto& table = *kv.second.as_table();
      for (const auto& item : table) {
        target.values.emplace(item.first.str(), toml_node_to_string(item.second));
      }

      auto& bucket = targets_by_name[target.name];
      const auto duplicate = std::find_if(bucket.begin(), bucket.end(), [&](const TargetConfig& existing) {
        return existing.use == target.use;
      });
      if (duplicate != bucket.end()) {
        throw std::runtime_error("duplicate target name for provider '" + target.use + "': " + target.name);
      }
      bucket.push_back(std::move(target));
    }
  }
}

} // namespace

ConfigPaths get_config_paths(const std::string& app_name) {
  const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME");
  std::filesystem::path root;
  if (xdg_config_home && *xdg_config_home != '\0') {
    root = std::filesystem::path(xdg_config_home) / app_name;
  } else {
    const char* home = std::getenv("HOME");
    if (!home || *home == '\0') throw std::runtime_error("HOME environment variable not set");
    root = std::filesystem::path(home) / ".config" / app_name;
  }

  ConfigPaths paths;
  paths.root_dir = root;
  paths.providers_dir = root / kProvidersDir;
  paths.targets_dir = root / kTargetsDir;
  paths.config_file = root / kConfigFile;
  return paths;
}

LoadedConfig load_config_tree(const ConfigPaths& paths) {
  LoadedConfig loaded;
  loaded.defaults = load_defaults_file(paths.config_file);
  load_provider_files(paths.providers_dir, loaded.providers);
  load_target_files(paths.targets_dir, loaded.targets_by_name);

  if (loaded.providers.empty()) throw std::runtime_error("no providers found in " + paths.providers_dir.string());
  if (loaded.targets_by_name.empty()) throw std::runtime_error("no targets found in " + paths.targets_dir.string());

  return loaded;
}

} // namespace manakan
