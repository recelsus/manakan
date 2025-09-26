// karing-style: pragma once, snake_case, namespace
#pragma once

#include <optional>
#include <string>
#include <utility>

namespace manakan {

std::optional<std::pair<std::string, std::string>> parse_arguments(int argc, char* argv[]);
std::string read_message_from_stdin();

} // namespace manakan
