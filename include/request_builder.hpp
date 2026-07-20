#pragma once

#include "models.hpp"
#include <string>
#include <unordered_map>

namespace manakan {

// A fully-normalized, serialized HTTP request: everything the Transport layer
// needs to execute a send, and everything `manakan config --json/--body/--curl`
// needs to render one without sending it. Building this is the Request Builder's
// responsibility so Transport stays a thin sender and diagnostics/output code
// never has to re-derive normalization or serialization rules.
struct BuiltHttpRequest {
  std::string method;      // lower-case, e.g. "post"
  std::string base_url;    // normalized scheme + host, no trailing slash
  std::string path;        // normalized, always starts with '/'
  std::unordered_map<std::string, std::string> headers;
  std::string content_type;
  std::string body; // serialized per content_type (JSON or form-urlencoded)
};

BuiltHttpRequest build_http_request(const ResolvedRequest& request);

} // namespace manakan
