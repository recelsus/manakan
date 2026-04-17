#pragma once

#include "models.hpp"

namespace manakan {

void print_usage(const std::string& app_name);
void print_version(const std::string& app_name);
bool prepare_runtime_environment(const ConfigPaths& paths);
int run_command(const std::string& app_name, CliOptions cli);

} // namespace manakan
