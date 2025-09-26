// karing-style: includes, namespace usage, snake_case helpers
#include "arguments.hpp"
#include "config_parser.hpp"
#include "helpers.hpp"
#include "httplib.h"
#include "json.hpp"
#include <string>
#include <unistd.h>

namespace {
constexpr const char* APP_NAME = "manakan";

void print_usage(const std::string& app_name) {
  std::cout << "Usage: " << app_name << " [-t target] [message]" << std::endl;
}
} // namespace

int main(int argc, char* argv[]) {
  using namespace manakan;

  if (argc > 4) {
    print_usage(APP_NAME);
    return 1;
  }

  auto args = parse_arguments(argc, argv);
  if (!args) {
    print_usage(APP_NAME);
    return 1;
  }

  auto [host_key, message] = args.value();
  if (message.empty() && !isatty(STDIN_FILENO)) message = read_message_from_stdin();
  if (message.empty()) {
    print_usage(APP_NAME);
    return 1;
  }

  try {
    const std::string config_path = get_config_file_path(APP_NAME);
    auto cfg = parse_toml_config(config_path, host_key);
    if (!cfg) return 1;

    const auto& config = *cfg;
    httplib::Client cli(config.url.c_str());

    // Headers and content type
    httplib::Headers headers;
    std::string content_type = "application/json";
    for (const auto& kv : config.header) {
      headers.insert(kv);
      if (to_lower(kv.first) == "content-type") content_type = kv.second;
    }

    // Body
    std::string body;
    const std::string& message_key = config.messageKey;
    if (content_type == "application/json") {
      nlohmann::json json_data;
      for (const auto& kv : config.data) json_data[kv.first] = kv.second;
      json_data[message_key] = message;
      body = json_data.dump();
    } else if (content_type == "application/x-www-form-urlencoded") {
      std::ostringstream oss;
      for (const auto& kv : config.data) {
        if (oss.tellp() > 0) oss << "&";
        oss << url_encode(kv.first) << "=" << url_encode(kv.second);
      }
      if (!message_key.empty()) {
        if (oss.tellp() > 0) oss << "&";
        oss << url_encode(message_key) << "=" << url_encode(message);
      }
      body = oss.str();
    }

    if (auto res = cli.Post(config.path.c_str(), headers, body, content_type)) {
      if (res->status >= 200 && res->status <= 204) {
        std::cout << "Send: " << message << std::endl;
        if (!res->body.empty()) std::cout << "Response: " << res->body << std::endl;
      } else {
        std::cout << "Response: " << res->status << std::endl;
      }
    }
  } catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << std::endl;
    return 1;
  }
  return 0;
}
