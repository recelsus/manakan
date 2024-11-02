#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include "arguments.hpp"

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
