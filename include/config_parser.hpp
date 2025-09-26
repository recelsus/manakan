// karing-style: pragma once, namespace, snake_case API
#pragma once

#include <optional>
#include <string>
#include <toml.hpp>
#include <unordered_map>

namespace manakan {

struct Config {
  std::string url;
  std::string path;
  std::unordered_map<std::string, std::string> header;
  std::unordered_map<std::string, std::string> data;
  std::string messageKey;
};

std::optional<Config> parse_toml_config(const std::string& filepath, const std::string& host_key);

} // namespace manakan
