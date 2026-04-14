#include "cli.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace manakan {

CliOptions parse_cli_options(int argc, char* argv[]) {
  CliOptions options;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      options.show_help = true;
      continue;
    }
    if (arg == "-v" || arg == "--version") {
      options.show_version = true;
      continue;
    }
    if (arg == "-p" || arg == "--provider") {
      if (i + 1 >= argc) throw std::runtime_error("missing value for --provider");
      options.provider = argv[++i];
      continue;
    }
    if (arg == "-t" || arg == "--target") {
      if (i + 1 >= argc) throw std::runtime_error("missing value for --target");
      options.target = argv[++i];
      continue;
    }
    if (arg == "-i" || arg == "--input") {
      if (i + 1 >= argc) throw std::runtime_error("missing value for --input");
      const std::string raw = argv[++i];
      const auto pos = raw.find('=');
      if (pos == std::string::npos || pos == 0 || pos + 1 >= raw.size()) {
        throw std::runtime_error("invalid --input format, expected key=value");
      }
      options.inputs[raw.substr(0, pos)] = raw.substr(pos + 1);
      continue;
    }
    if (!arg.empty() && arg[0] == '-') {
      throw std::runtime_error("unknown option: " + arg);
    }
    options.positional_args.push_back(arg);
  }

  return options;
}

std::string read_message_from_stdin() {
  std::ostringstream oss;
  oss << std::cin.rdbuf();
  std::string message = oss.str();
  if (!message.empty() && message.back() == '\n') message.pop_back();
  return message;
}

} // namespace manakan
