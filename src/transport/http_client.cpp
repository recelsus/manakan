#include "transport.hpp"
#include "httplib.h"
#include "request_builder.hpp"
#include <stdexcept>

namespace manakan {
namespace {

httplib::Headers to_headers(const std::unordered_map<std::string, std::string>& headers) {
  httplib::Headers out;
  for (const auto& kv : headers) out.emplace(kv.first, kv.second);
  return out;
}

} // namespace

HttpResponse send_request(const ResolvedRequest& request) {
  const BuiltHttpRequest built = build_http_request(request);
  httplib::Client client(built.base_url.c_str());
  const auto headers = to_headers(built.headers);

  httplib::Result response;
  if (built.method == "get") response = client.Get(built.path.c_str(), headers);
  else if (built.method == "post") response = client.Post(built.path.c_str(), headers, built.body, built.content_type.c_str());
  else if (built.method == "put") response = client.Put(built.path.c_str(), headers, built.body, built.content_type.c_str());
  else if (built.method == "patch") response = client.Patch(built.path.c_str(), headers, built.body, built.content_type.c_str());
  else if (built.method == "delete") response = client.Delete(built.path.c_str(), headers, built.body, built.content_type.c_str());
  else throw std::runtime_error("unsupported HTTP method: " + request.method);

  if (!response) {
    throw std::runtime_error(
        "HTTP request failed: " + httplib::to_string(response.error()) +
        " (method=" + request.method +
        ", base_url=" + built.base_url +
        ", path=" + built.path + ")");
  }

  HttpResponse out;
  out.status = response->status;
  out.body = response->body;
  return out;
}

} // namespace manakan
