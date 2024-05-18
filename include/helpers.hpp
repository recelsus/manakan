#ifndef HELPERS_HPP
#define HELPERS_HPP

#include <string>

std::string toLower(const std::string &str);
std::string urlEncode(const std::string &value);
std::string getConfigFilePath(const std::string &appName);

#endif // HELPERS_HPP

