#pragma once

#include "models.hpp"

namespace manakan {

CliOptions parse_cli_options(int argc, char* argv[]);
std::string read_message_from_stdin();

} // namespace manakan
