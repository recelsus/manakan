#include <helpers.hpp>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cstdlib>

std::string toLower(const std::string &str) {
  std::string lowerStr = str;
  std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
  return lowerStr;
}

std::string urlEncode(const std::string &value) {
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

std::string getConfigFilePath(const std::string &appName) {
  const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
  std::string configFilePath;

  std::ostringstream oss;
  oss << "/" << appName << "/" << appName << ".toml";
  const std::string applicationConfigPath = oss.str();
  
  if (xdgConfigHome) {
    configFilePath = std::string(xdgConfigHome) + applicationConfigPath;
  } else {
    const char* home = std::getenv("HOME");
    if(!home) {
      throw std::runtime_error("HOME environment variable not set");
    }
    configFilePath = std::string(home) + "/.config" + applicationConfigPath;
  }

  return configFilePath;
}
