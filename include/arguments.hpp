#ifndef ARGUMENTS_HPP
#define ARGUMENTS_HPP

#include <string>
#include <optional>
#include <utility>

std::optional<std::pair<std::string, std::string>> parseArguments(int argc, char *argv[]);
std::string readMessageFromStdin();

#endif
