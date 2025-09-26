// karing-style: namespace, snake_case
#include "helpers.hpp"
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace manakan {

std::string to_lower(const std::string& s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(), ::tolower);
  return out;
}

std::string url_encode(const std::string& value) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;
  for (char c : value) {
    if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << c;
    } else {
      escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
    }
  }
  return escaped.str();
}

std::string get_config_file_path(const std::string& app_name) {
  const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME");
  std::ostringstream oss;
  oss << "/" << app_name << "/" << app_name << ".toml";
  const std::string app_cfg_rel = oss.str();

  if (xdg_config_home) {
    return std::string(xdg_config_home) + app_cfg_rel;
  }
  const char* home = std::getenv("HOME");
  if (!home) throw std::runtime_error("HOME environment variable not set");
  return std::string(home) + "/.config" + app_cfg_rel;
}

} // namespace manakan

