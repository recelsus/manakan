#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace manakan {

enum class Command { Send, Config };

struct CliOptions {
  Command command = Command::Send;
  std::optional<std::string> provider;
  std::optional<std::string> target;
  std::unordered_map<std::string, std::string> inputs;
  std::vector<std::string> positional_args;
  bool show_help = false;
  bool show_version = false;
  // `manakan config` render modes; mutually informative, not mutually exclusive
  // at the parser level (the app layer decides precedence).
  bool config_json = false;
  bool config_body = false;
  bool config_curl = false;
  bool config_check = false;
};

struct ConfigPaths {
  std::filesystem::path root_dir;
  std::filesystem::path providers_dir;
  std::filesystem::path targets_dir;
  std::filesystem::path config_file;
};

struct AppDefaults {
  std::optional<std::string> default_provider;
};

// TomlValue is a structure-preserving value tree: a TOML value is either a
// scalar (kept as its original string form), a table, or an array. Providers
// and Targets are decoded into this tree and stay tree-shaped through
// merging and placeholder resolution; only the request builder flattens it
// into an HTTP request.
class TomlValue {
 public:
  using Table = std::map<std::string, TomlValue>;
  using Array = std::vector<TomlValue>;

  enum class Kind { Scalar, Table, Array };

  TomlValue() : kind_(Kind::Table) {}
  explicit TomlValue(std::string scalar) : kind_(Kind::Scalar), scalar_(std::move(scalar)) {}
  explicit TomlValue(Table table) : kind_(Kind::Table), table_(std::move(table)) {}
  explicit TomlValue(Array array) : kind_(Kind::Array), array_(std::move(array)) {}

  Kind kind() const { return kind_; }
  bool is_scalar() const { return kind_ == Kind::Scalar; }
  bool is_table() const { return kind_ == Kind::Table; }
  bool is_array() const { return kind_ == Kind::Array; }

  const std::string& scalar() const {
    if (kind_ != Kind::Scalar) throw std::runtime_error("TomlValue is not a scalar");
    return scalar_;
  }

  const Table& table() const {
    if (kind_ != Kind::Table) throw std::runtime_error("TomlValue is not a table");
    return table_;
  }
  Table& table() {
    if (kind_ != Kind::Table) throw std::runtime_error("TomlValue is not a table");
    return table_;
  }

  const Array& array() const {
    if (kind_ != Kind::Array) throw std::runtime_error("TomlValue is not an array");
    return array_;
  }
  Array& array() {
    if (kind_ != Kind::Array) throw std::runtime_error("TomlValue is not an array");
    return array_;
  }

  // Direct child lookup; returns nullptr if this is not a table or the key is absent.
  const TomlValue* find(const std::string& key) const {
    if (kind_ != Kind::Table) return nullptr;
    auto it = table_.find(key);
    return it == table_.end() ? nullptr : &it->second;
  }

 private:
  Kind kind_;
  std::string scalar_;
  Table table_;
  Array array_;
};

struct ProviderConfig {
  std::string name;
  std::string source_file;
  // Top-level keys a Target file is allowed to write into (e.g. "request", "headers", "data").
  std::vector<std::string> top_level_sections;
  // Dotted paths (e.g. "request.method") protected by `const`/`c`; Targets may not write
  // to a path equal to, or nested under, any of these.
  std::vector<std::string> const_paths;
  // Normalized provider tree (const/c wrappers already promoted into their parent table).
  TomlValue tree;
};

struct TargetConfig {
  std::string use;   // provider name this target belongs to
  std::string name;  // target name (the table key within the target file)
  std::string source_file;
  bool is_default = false;
  // Values restricted to the provider's top_level_sections; validated at load time.
  TomlValue tree;
};

struct LoadedConfig {
  AppDefaults defaults;
  std::unordered_map<std::string, ProviderConfig> providers;
  std::vector<TargetConfig> targets;
  // Messages for files that failed to parse/validate but did not block the run
  // because they were unrelated to the selected provider/target.
  std::vector<std::string> warnings;
};

struct ResolvedRequest {
  std::string provider_name;
  std::string target_name;
  std::string method;
  std::string base_url;
  std::string path;
  std::unordered_map<std::string, std::string> headers;
  // Fully placeholder-resolved but still tree-shaped; serialization is the
  // request builder's responsibility, not the resolver's.
  TomlValue body;
};

struct HttpResponse {
  int status = 0;
  std::string body;
};

} // namespace manakan
