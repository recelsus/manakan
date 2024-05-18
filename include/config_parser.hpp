#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <string>
#include <unordered_map>
#include <optional>
#include <toml.hpp>

struct Config {
  std::string url;
  std::string path;
  std::unordered_map<std::string, std::string> header;
  std::unordered_map<std::string, std::string> data;
  std::string messageKey;
};

std::optional<Config> parseTomlConfig(const std::string& filepath, const std::string& hostKey);
#endif
