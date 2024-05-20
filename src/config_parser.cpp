#include "config_parser.hpp"
#include <iostream>
#include <fstream>
#include <regex>
#include <vector>

std::vector<std::string> extractHostKeys(const std::string& filepath) {
  std::vector<std::string> hostKeys;
  std::ifstream file(filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Could not open config file");
  }

  std::string line;
  std::regex hostRegex(R"(^\[host\.(.+?)\])");
  std::smatch match;

  while (std::getline(file, line)) {
    if (std::regex_search(line, match, hostRegex)) {
      hostKeys.push_back(match[1].str());
    }
  }
  return hostKeys;
}

std::optional<Config> parseTomlConfig(const std::string& filepath, const std::string& hostKey) {

  try {
    auto config = toml::parse_file(filepath);
    Config result;

    std::string effectiveHostKey = hostKey;

    if (hostKey == "default-host") {
      std::vector<std::string> hostKeys = extractHostKeys(filepath);
      
      for (const auto& key : hostKeys) {
      }

      if (!hostKey.empty()) {
        effectiveHostKey = hostKeys.front();
      }
    }

    std::cout << effectiveHostKey << std::endl;

    auto hostTable = config["host"][effectiveHostKey];
    if(!hostTable.is_table()) {
      throw std::runtime_error("Host key no found in config file");
    } 

    if (auto url = hostTable["url"].value<std::string>(); url) {
      result.url = *url;
    } else {
      throw std::runtime_error("URL not found in config file");
    } 

    if (auto path = hostTable["path"].value<std::string>(); path) {
      result.path = *path; 
    } else {
      result.path = "/";
    }

    if (auto headerTable = hostTable["header"].as_table(); headerTable) {
      for (const auto& kv : *headerTable) {
        if (auto value = kv.second.value<std::string>(); value) {
          result.header.emplace(kv.first.str(), *value);
        }
      }
    }

    if (auto dataTable = hostTable["data"].as_table(); dataTable) {
      for (const auto& kv : *dataTable) {
        std::string value_str;
      if (auto value = kv.second.value<std::string>()) {
        value_str = *value;
      } else if (auto value = kv.second.value<int>()) {
        value_str = std::to_string(*value);
      } else if (auto value = kv.second.value<double>()) {
        value_str = std::to_string(*value);
      }
      result.data.emplace(kv.first.str(), value_str);
      }
   }

    if (auto messageKey = hostTable["message"].value<std::string>(); messageKey) {
      result.messageKey = *messageKey;
    } else {
      throw std::runtime_error("Message key not found in config file");
    }

    return result;
  } catch(const std::exception& ex) {
    std::cerr << "Error parsing config file: " << ex.what() << std::endl;
    return std::nullopt;
  }
}
