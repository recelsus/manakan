#pragma once

#include "models.hpp"

namespace manakan {

ConfigPaths get_config_paths(const std::string& app_name);
LoadedConfig load_config_tree(const ConfigPaths& paths);

} // namespace manakan
