#include <iostream>
#include <string>
#include <unistd.h>
#include "httplib.h"
#include "json.hpp"
#include "config_parser.hpp"
#include "helpers.hpp"

const std::string APP_NAME = "manakan";

void printUsage(const std::string &appName) {
  std::cout << "Usage: " << appName << " [-t target] [message]" << std::endl;
}

std::optional<std::pair<std::string, std::string>> parseArguments(int argc, char *argv[]) {
  std::string hostKey = "$$default$$";
  std::string message;

  if (argc == 1) {
    if (isatty(STDIN_FILENO)) {
      return std::nullopt;
    }
    message = "";
  } else if (argc == 2 && std::string(argv[1]) != "-t") {
    message = argv[1];
  } else if (argc == 3 && std::string(argv[1]) == "-t") {
    hostKey = argv[2];
    message = "";
  } else if (argc == 4 && std::string(argv[1]) == "-t") {
    hostKey = argv[2];
    message = argv[3];
  } else {
    return std::nullopt;
  }
  return std::make_pair(hostKey, message);
}

std::string readMessageFromStdin() {
  std::ostringstream oss;
  oss << std::cin.rdbuf();
  std::string message = oss.str();

  if (!message.empty() && message.back() == '\n') {
    message.pop_back();
  }
  return message;
}

int main(int argc, char *argv[]) {

  if (argc > 4) {
    printUsage(APP_NAME);
    return 1;
  }

  auto args = parseArguments(argc, argv);
  if (!args) {
    printUsage(APP_NAME);
    return 1;
  }

  auto [hostKey, message] = args.value();

  if (message.empty() && !isatty(STDIN_FILENO)) {
    message = readMessageFromStdin();
  }

  if (message.empty()) {
    printUsage(APP_NAME);
    return 1;
  }

  try {
    std::string configPath = getConfigFilePath(APP_NAME);
    auto configOpt = parseTomlConfig(configPath, hostKey);
    
    if (configOpt) {
      Config config = configOpt.value();
      
      const std::string url = config.url;
      const std::string path = config.path;
      
      httplib::Client cli(url.c_str());

      httplib::Headers headers;
      for (const auto& kv : config.header) {
        headers.insert(kv);
      }

      std::string contentType = "application/json";
      for (const auto& kv : config.header) {
        headers.insert(kv);
        if (toLower(kv.first) == "content-type") {
          contentType = kv.second;
        }
      }

      const std::string messageKey = config.messageKey;

      std::string body;
      if (contentType == "application/json") {
        nlohmann::json jsonData;
        for (const auto& kv : config.data) {
          jsonData[kv.first] = kv.second;
        }
        jsonData[messageKey] = message;
        body = jsonData.dump();
      } else if (contentType == "application/x-www-form-urlencoded") {
        std::ostringstream oss;
        for (const auto& kv : config.data) {
          if(oss.tellp() > 0) {
            oss << "&";
          }
          oss << urlEncode(kv.first) << "=" << urlEncode(kv.second);
        }

        if (!messageKey.empty()) {
          if (oss.tellp() > 0) {
            oss << "&";
          }
            oss << urlEncode(messageKey) << "=" << urlEncode(message);
        }
        body = oss.str();       }

      if (auto res = cli.Post(config.path.c_str(), headers, body, contentType)) {
        if (res->status >= 200 && res->status <= 204) {
          if (!res->body.empty()) {
            std::cout << "Send: " << message << std::endl;
            std::cout << "Response: " << res->body << std::endl;
          } else {
            std::cout << "Send: " << message << std::endl;
          }
        } else {
          std::cout << "Response: " << res->status << std::endl;
        }
      }
    }    

  } catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << std::endl;
    return 1;
  }
  return 0;
}
