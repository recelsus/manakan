#include "transport.hpp"
#include "helpers.hpp"
#include "httplib.h"
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

std::string serialize_body(const std::unordered_map<std::string, std::string>& body, const std::string& content_type) {
  if (body.empty()) return "";

  if (content_type == "application/x-www-form-urlencoded") {
    std::ostringstream oss;
    for (auto it = body.begin(); it != body.end(); ++it) {
      if (it != body.begin()) oss << "&";
      oss << url_encode(it->first) << "=" << url_encode(it->second);
    }
    return oss.str();
  }

  nlohmann::json json_data;
  for (const auto& kv : body) json_data[kv.first] = kv.second;
  return json_data.dump();
}

httplib::Headers to_headers(const std::unordered_map<std::string, std::string>& headers) {
  httplib::Headers out;
  for (const auto& kv : headers) out.emplace(kv.first, kv.second);
  return out;
}

} // namespace

HttpResponse send_request(const ResolvedRequest& request) {
  const std::string normalized_base_url = normalize_base_url(request.base_url);
  const std::string normalized_path = normalize_path(request.path);
  httplib::Client client(normalized_base_url.c_str());
  const auto headers = to_headers(request.headers);
  const std::string content_type = detect_content_type(request.headers);
  const std::string body = serialize_body(request.body, content_type);
  const std::string method = to_lower(request.method);

  httplib::Result response;
  if (method == "get") response = client.Get(normalized_path.c_str(), headers);
  else if (method == "post") response = client.Post(normalized_path.c_str(), headers, body, content_type.c_str());
  else if (method == "put") response = client.Put(normalized_path.c_str(), headers, body, content_type.c_str());
  else if (method == "patch") response = client.Patch(normalized_path.c_str(), headers, body, content_type.c_str());
  else if (method == "delete") response = client.Delete(normalized_path.c_str(), headers, body, content_type.c_str());
  else throw std::runtime_error("unsupported HTTP method: " + request.method);

  if (!response) {
    throw std::runtime_error(
        "HTTP request failed: " + httplib::to_string(response.error()) +
        " (method=" + request.method +
        ", base_url=" + normalized_base_url +
        ", path=" + normalized_path + ")");
  }

  HttpResponse out;
  out.status = response->status;
  out.body = response->body;
  return out;
}

} // namespace manakan
