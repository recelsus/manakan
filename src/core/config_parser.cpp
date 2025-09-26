// karing-style: namespace, snake_case, minor robustness fixes
#include "config_parser.hpp"
#include <fstream>
#include <iostream>
#include <regex>
#include <vector>

namespace manakan {

static std::vector<std::string> extract_host_keys(const std::string& filepath) {
  std::vector<std::string> keys;
  std::ifstream file(filepath);
  if (!file.is_open()) throw std::runtime_error("Could not open config file");
  std::string line;
  std::regex host_re(R"(^\[host\.(.+?)\])");
  std::smatch m;
  while (std::getline(file, line)) {
    if (std::regex_search(line, m, host_re)) keys.push_back(m[1].str());
  }
  return keys;
}

std::optional<Config> parse_toml_config(const std::string& filepath, const std::string& host_key) {
  try {
    auto cfg = toml::parse_file(filepath);
    Config out;

    std::string effective = host_key;
    if (host_key == "$$default$$") {
      auto keys = extract_host_keys(filepath);
      if (!keys.empty()) effective = keys.front();
    }

    auto host_table = cfg["host"][effective];
    if (!host_table.is_table()) throw std::runtime_error("Host key not found in config file");

    if (auto url = host_table["url"].value<std::string>(); url) out.url = *url; else throw std::runtime_error("URL not found in config file");
    if (auto path = host_table["path"].value<std::string>(); path) out.path = *path; else out.path = "/";

    if (auto header = host_table["header"].as_table(); header) {
      for (const auto& kv : *header) {
        if (auto v = kv.second.value<std::string>(); v) out.header.emplace(kv.first.str(), *v);
      }
    }

    if (auto data = host_table["data"].as_table(); data) {
      for (const auto& kv : *data) {
        std::string vstr;
        if (auto v = kv.second.value<std::string>()) vstr = *v;
        else if (auto v2 = kv.second.value<int>()) vstr = std::to_string(*v2);
        else if (auto v3 = kv.second.value<double>()) vstr = std::to_string(*v3);
        out.data.emplace(kv.first.str(), vstr);
      }
    }

    if (auto mkey = host_table["message"].value<std::string>(); mkey) out.messageKey = *mkey; else throw std::runtime_error("Message key not found in config file");

    return out;
  } catch (const std::exception& ex) {
    std::cerr << "Error parsing config file: " << ex.what() << std::endl;
    return std::nullopt;
  }
}

} // namespace manakan

