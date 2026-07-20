#include "request_builder.hpp"
#include "helpers.hpp"
#include "json.hpp"
#include <regex>
#include <sstream>
#include <stdexcept>

namespace manakan {
namespace {

std::string normalize_base_url(const std::string& base_url) {
  static const std::regex base_url_re(R"(^(https?://[^/\s]+)(/.*)?$)", std::regex::icase);
  std::smatch match;
  if (!std::regex_match(base_url, match, base_url_re)) {
    throw std::runtime_error("invalid base_url: " + base_url);
  }

  const std::string host = match[1].str();
  const std::string suffix = match[2].matched ? match[2].str() : "";
  if (!suffix.empty() && suffix != "/") {
    throw std::runtime_error("base_url must not contain a path: " + base_url);
  }
  return host;
}

std::string normalize_path(const std::string& path) {
  if (path.empty()) return "/";
  if (path.front() == '/') return path;
  return "/" + path;
}

std::string detect_content_type(const std::unordered_map<std::string, std::string>& headers) {
  for (const auto& kv : headers) {
    if (to_lower(kv.first) == "content-type") return kv.second;
  }
  return "application/json";
}

nlohmann::json toml_value_to_json(const TomlValue& value) {
  if (value.is_scalar()) return value.scalar();
  if (value.is_array()) {
    nlohmann::json array = nlohmann::json::array();
    for (const auto& item : value.array()) array.push_back(toml_value_to_json(item));
    return array;
  }
  nlohmann::json object = nlohmann::json::object();
  for (const auto& kv : value.table()) object[kv.first] = toml_value_to_json(kv.second);
  return object;
}

std::string serialize_form(const TomlValue& body) {
  if (!body.is_table()) throw std::runtime_error("form-encoded body must be a flat table of scalar values");

  std::ostringstream oss;
  bool first = true;
  for (const auto& kv : body.table()) {
    if (!kv.second.is_scalar()) {
      throw std::runtime_error("form-encoded body field '" + kv.first + "' must be a scalar value, not nested");
    }
    if (!first) oss << "&";
    first = false;
    oss << url_encode(kv.first) << "=" << url_encode(kv.second.scalar());
  }
  return oss.str();
}

std::string serialize_body(const TomlValue& body, const std::string& content_type) {
  if (body.is_table() && body.table().empty()) return "";
  if (content_type == "application/x-www-form-urlencoded") return serialize_form(body);
  return toml_value_to_json(body).dump();
}

} // namespace

BuiltHttpRequest build_http_request(const ResolvedRequest& request) {
  BuiltHttpRequest built;
  built.method = to_lower(request.method);
  built.base_url = normalize_base_url(request.base_url);
  built.path = normalize_path(request.path);
  built.headers = request.headers;
  built.content_type = detect_content_type(request.headers);
  built.body = serialize_body(request.body, built.content_type);
  return built;
}

} // namespace manakan
