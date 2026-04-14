// karing-style: pragma once, snake_case, namespace
#pragma once

#include <string>

namespace manakan {

std::string to_lower(const std::string& s);
std::string trim(const std::string& s);
std::string url_encode(const std::string& value);

} // namespace manakan
