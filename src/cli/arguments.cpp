// karing-style: snake_case names, namespace
#include "arguments.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace manakan {

std::optional<std::pair<std::string, std::string>> parse_arguments(int argc, char* argv[]) {
  std::string host_key = "$$default$$";
  std::string message;

  if (argc == 1) {
    if (isatty(STDIN_FILENO)) return std::nullopt;
    message.clear();
  } else if (argc == 2 && std::string(argv[1]) != "-t") {
    message = argv[1];
  } else if (argc == 3 && std::string(argv[1]) == "-t") {
    host_key = argv[2];
    message.clear();
  } else if (argc == 4 && std::string(argv[1]) == "-t") {
    host_key = argv[2];
    message = argv[3];
  } else {
    return std::nullopt;
  }
  return std::make_pair(host_key, message);
}

std::string read_message_from_stdin() {
  std::ostringstream oss;
  oss << std::cin.rdbuf();
  std::string message = oss.str();
  if (!message.empty() && message.back() == '\n') message.pop_back();
  return message;
}

} // namespace manakan

