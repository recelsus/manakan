// karing-style: pragma once, snake_case, namespace
#pragma once

#include <string>

namespace manakan {

std::string to_lower(const std::string& s);
std::string url_encode(const std::string& value);
std::string get_config_file_path(const std::string& app_name);

} // namespace manakan
