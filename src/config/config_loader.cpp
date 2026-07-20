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

std::string toml_scalar_to_string(const toml::node& node) {
  if (auto value = node.value<std::string>()) return *value;
  if (auto value = node.value<int64_t>()) return std::to_string(*value);
  if (auto value = node.value<double>()) return std::to_string(*value);
  if (auto value = node.value<bool>()) return *value ? "true" : "false";
  throw std::runtime_error("unsupported TOML value type");
}

TomlValue toml_node_to_value(const toml::node& node) {
  if (node.is_table()) {
    TomlValue::Table table;
    for (const auto& kv : *node.as_table()) {
      table.emplace(std::string(kv.first.str()), toml_node_to_value(kv.second));
    }
    return TomlValue(std::move(table));
  }
  if (node.is_array()) {
    TomlValue::Array array;
    for (const auto& item : *node.as_array()) array.push_back(toml_node_to_value(item));
    return TomlValue(std::move(array));
  }
  return TomlValue(toml_scalar_to_string(node));
}

TomlValue toml_table_to_value(const toml::table& root) {
  TomlValue::Table table;
  for (const auto& kv : root) table.emplace(std::string(kv.first.str()), toml_node_to_value(kv.second));
  return TomlValue(std::move(table));
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

// Records `path` and, if `value` is a table, every path nested inside it. Used to
// mark every location touched by a `const`/`c` promotion as protected, at any depth.
void collect_paths(const TomlValue& value, const std::string& path, std::vector<std::string>& out) {
  out.push_back(path);
  if (value.is_table()) {
    for (const auto& kv : value.table()) collect_paths(kv.second, path + "." + kv.first, out);
  }
}

// Merges a promoted `const`/`c` subtree into its parent table. Existing sibling
// tables are merged into recursively (partial protection); anything else that
// already exists at the same path is a structural conflict.
void merge_const_into(TomlValue::Table& target, const TomlValue::Table& source, const std::string& prefix,
                       const std::string& provider_name, std::vector<std::string>& const_paths) {
  for (const auto& kv : source) {
    const std::string path = prefix.empty() ? kv.first : prefix + "." + kv.first;
    auto it = target.find(kv.first);
    if (it == target.end()) {
      auto [inserted, _] = target.emplace(kv.first, kv.second);
      collect_paths(inserted->second, path, const_paths);
      continue;
    }
    if (it->second.is_table() && kv.second.is_table()) {
      merge_const_into(it->second.table(), kv.second.table(), path, provider_name, const_paths);
      continue;
    }
    throw std::runtime_error("provider '" + provider_name + "' has conflicting definitions for '" + path +
                              "' (const and non-const values disagree on type)");
  }
}

// Recursively promotes `const`/`c` tables into their parent table and records every
// path they touch as protected. Recurses into ordinary nested tables first so that
// deeper const blocks are already resolved before a parent-level const is promoted.
void normalize_const(TomlValue::Table& table, const std::string& prefix, const std::string& provider_name,
                      std::vector<std::string>& const_paths) {
  for (auto& kv : table) {
    if (kv.second.is_table()) {
      const std::string child_prefix = prefix.empty() ? kv.first : prefix + "." + kv.first;
      normalize_const(kv.second.table(), child_prefix, provider_name, const_paths);
    }
  }

  for (const char* const_key : {"const", "c"}) {
    auto it = table.find(const_key);
    if (it == table.end()) continue;
    if (!it->second.is_table()) {
      throw std::runtime_error("provider '" + provider_name + "' has '" + const_key + "' at '" + prefix +
                                "' that is not a table");
    }
    TomlValue::Table promoted = std::move(it->second.table());
    table.erase(it);
    merge_const_into(table, promoted, prefix, provider_name, const_paths);
  }
}

AppDefaults load_defaults_file(const std::filesystem::path& path) {
  AppDefaults defaults;
  if (!std::filesystem::exists(path)) return defaults;

  auto cfg = toml::parse_file(path.string());
  if (auto value = cfg["default_provider"].value<std::string>()) defaults.default_provider = *value;
  return defaults;
}

void load_provider_files(const std::filesystem::path& dir, std::unordered_map<std::string, ProviderConfig>& providers,
                          std::vector<std::string>& warnings) {
  for (const auto& path : list_toml_files(dir)) {
    try {
      auto cfg = toml::parse_file(path.string());
      TomlValue root = toml_table_to_value(cfg);

      const TomlValue* name_value = root.find("name");
      if (!name_value || !name_value->is_scalar() || name_value->scalar().empty()) {
        throw std::runtime_error("provider file missing name: " + path.string());
      }
      const std::string name = name_value->scalar();

      ProviderConfig provider;
      provider.name = name;
      provider.source_file = path.string();

      normalize_const(root.table(), "", name, provider.const_paths);

      const TomlValue* request = root.find("request");
      if (!request || !request->is_table()) {
        throw std::runtime_error("provider '" + name + "' missing [request] section: " + path.string());
      }
      auto require_scalar = [&](const char* key) -> std::string {
        const TomlValue* value = request->find(key);
        if (!value || !value->is_scalar() || value->scalar().empty()) {
          throw std::runtime_error("provider '" + name + "' missing request." + key + ": " + path.string());
        }
        return value->scalar();
      };
      require_scalar("method");
      require_scalar("base_url");
      if (const TomlValue* path_value = request->find("path"); !path_value || !path_value->is_scalar()) {
        throw std::runtime_error("provider '" + name + "' missing request.path: " + path.string());
      }

      for (const auto& kv : root.table()) {
        if (kv.first == "name") continue;
        if (kv.second.is_table()) provider.top_level_sections.push_back(kv.first);
      }

      provider.tree = std::move(root);

      if (!providers.emplace(name, std::move(provider)).second) {
        warnings.push_back("duplicate provider name '" + name + "' in " + path.string() + " (ignored, first definition kept)");
      }
    } catch (const std::exception& ex) {
      warnings.push_back(std::string("provider file error in ") + path.string() + ": " + ex.what());
    }
  }
}

void load_target_files(const std::filesystem::path& dir, const std::unordered_map<std::string, ProviderConfig>& providers,
                        std::vector<TargetConfig>& targets, std::vector<std::string>& warnings) {
  std::set<std::string> defaults_seen_for_use;
  std::set<std::pair<std::string, std::string>> identities_seen;

  for (const auto& path : list_toml_files(dir)) {
    try {
      auto cfg = toml::parse_file(path.string());
      TomlValue root = toml_table_to_value(cfg);

      const TomlValue* use_value = root.find("use");
      if (!use_value || !use_value->is_scalar() || use_value->scalar().empty()) {
        throw std::runtime_error("target file missing use: " + path.string());
      }
      const std::string use = use_value->scalar();

      std::string default_name;
      if (const TomlValue* default_value = root.find("default"); default_value && default_value->is_scalar()) {
        default_name = default_value->scalar();
      }

      auto provider_it = providers.find(use);
      if (provider_it == providers.end()) {
        warnings.push_back("target file " + path.string() + " references unknown provider '" + use + "'");
        continue;
      }
      const ProviderConfig& provider = provider_it->second;
      const std::set<std::string> allowed_sections(provider.top_level_sections.begin(), provider.top_level_sections.end());

      for (const auto& kv : root.table()) {
        if (kv.first == "use" || kv.first == "default") continue;
        if (!kv.second.is_table()) continue;

        TargetConfig target;
        target.name = kv.first;
        target.use = use;
        target.source_file = path.string();

        bool section_error = false;
        for (const auto& section_kv : kv.second.table()) {
          if (allowed_sections.count(section_kv.first) == 0) {
            warnings.push_back("target '" + target.name + "' in " + path.string() + " writes to section '" +
                                section_kv.first + "' not declared by provider '" + use + "' (target ignored)");
            section_error = true;
            break;
          }
        }
        if (section_error) continue;

        const auto identity = std::make_pair(use, target.name);
        if (!identities_seen.insert(identity).second) {
          warnings.push_back("duplicate target '" + target.name + "' for provider '" + use + "' in " + path.string() +
                              " (ignored, first definition kept)");
          continue;
        }

        if (!default_name.empty() && target.name == default_name) {
          if (defaults_seen_for_use.count(use)) {
            warnings.push_back("provider '" + use + "' already has a default target; default in " + path.string() +
                                " ignored");
          } else {
            target.is_default = true;
            defaults_seen_for_use.insert(use);
          }
        }

        target.tree = kv.second;
        targets.push_back(std::move(target));
      }
    } catch (const std::exception& ex) {
      warnings.push_back(std::string("target file error in ") + path.string() + ": " + ex.what());
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
  load_provider_files(paths.providers_dir, loaded.providers, loaded.warnings);
  load_target_files(paths.targets_dir, loaded.providers, loaded.targets, loaded.warnings);

  if (loaded.providers.empty()) throw std::runtime_error("no providers found in " + paths.providers_dir.string());
  if (loaded.targets.empty()) throw std::runtime_error("no targets found in " + paths.targets_dir.string());

  return loaded;
}

} // namespace manakan
