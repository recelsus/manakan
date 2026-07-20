#pragma once

#include "models.hpp"

namespace manakan {

// Runs `manakan config --check`: static validation of the whole config tree
// (every provider, every target) without sending anything and without
// requiring runtime input (env/arg/argv). Prints a report and returns an exit
// code: 0 if clean, 1 if any warning or structural problem was found.
int run_config_check(const ConfigPaths& paths, const LoadedConfig& config);

} // namespace manakan
