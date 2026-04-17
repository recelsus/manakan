#include "config_loader.hpp"
#include "request_resolver.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct TempDir {
  fs::path path;

  TempDir() {
    path = fs::temp_directory_path() / fs::path("manakan-tests-" + std::to_string(std::rand()));
    fs::create_directories(path);
  }

  ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
};

void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

void write_file(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());
  std::ofstream ofs(path);
  if (!ofs) throw std::runtime_error("failed to open file for writing: " + path.string());
  ofs << content;
}

manakan::ConfigPaths make_paths(const fs::path& root) {
  manakan::ConfigPaths paths;
  paths.root_dir = root;
  paths.providers_dir = root / "providers";
  paths.targets_dir = root / "targets";
  paths.config_file = root / "config.toml";
  return paths;
}

void test_config_loader_allows_same_target_name_across_providers() {
  TempDir dir;
  const auto root = dir.path / "manakan";
  write_file(root / "providers" / "discord.toml",
             "name = \"discord\"\n"
             "method = \"POST\"\n"
             "base_url = \"https://discord.com\"\n"
             "path = \"/api/webhooks{{webhook_path}}\"\n"
             "[body]\n"
             "content = \"{{argv.1}}\"\n");
  write_file(root / "providers" / "chatwork.toml",
             "name = \"chatwork\"\n"
             "method = \"POST\"\n"
             "base_url = \"https://api.chatwork.com\"\n"
             "path = \"/v2/rooms/{{room_id}}/messages\"\n"
             "[body]\n"
             "body = \"{{argv.1}}\"\n");
  write_file(root / "targets" / "discord.toml",
             "use = \"discord\"\n"
             "[default]\n"
             "webhook_path = \"/abc\"\n");
  write_file(root / "targets" / "chatwork.toml",
             "use = \"chatwork\"\n"
             "[default]\n"
             "room_id = \"123\"\n");

  const auto loaded = manakan::load_config_tree(make_paths(root));
  require(loaded.targets_by_name.count("default") == 1, "expected grouped target bucket");
  require(loaded.targets_by_name.at("default").size() == 2, "expected two targets named default");
}

void test_resolver_prefers_cli_target_and_provider_values() {
  TempDir dir;
  const auto root = dir.path / "manakan";
  write_file(root / "providers" / "discord.toml",
             "name = \"discord\"\n"
             "method = \"POST\"\n"
             "base_url = \"https://discord.com\"\n"
             "path = \"{{webhook_path}}\"\n"
             "[body]\n"
             "username = \"{{username}}\"\n"
             "content = \"{{argv.1}}\"\n");
  write_file(root / "targets" / "discord.toml",
             "use = \"discord\"\n"
             "[default]\n"
             "webhook_path = \"/target\"\n"
             "username = \"target-user\"\n");
  write_file(root / "config.toml",
             "default_provider = \"discord\"\n"
             "default_target = \"default\"\n");

  const auto loaded = manakan::load_config_tree(make_paths(root));
  manakan::CliOptions cli;
  cli.inputs["username"] = "cli-user";
  cli.inputs["content"] = "cli-content";
  cli.positional_args.push_back("argv-content");

  const auto request = manakan::resolve_request(loaded, cli);
  require(request.provider_name == "discord", "expected discord provider");
  require(request.target_name == "default", "expected default target");
  require(request.path == "/target", "target should override provider value");
  require(request.body.at("username") == "cli-user", "--input should override target value");
  require(request.body.at("content") == "cli-content", "--input should override argv");
}

void test_resolver_handles_env_arg_and_argv_placeholders() {
  TempDir dir;
  const auto root = dir.path / "manakan";
  write_file(root / "providers" / "discord.toml",
             "name = \"discord\"\n"
             "method = \"POST\"\n"
             "base_url = \"https://discord.com\"\n"
             "path = \"{{env.TEST_WEBHOOK_PATH}}\"\n"
             "[body]\n"
             "username = \"{{arg.username}}\"\n"
             "content = \"{{argv.1}}\"\n");
  write_file(root / "targets" / "discord.toml",
             "use = \"discord\"\n"
             "[default]\n"
             "unused = \"ok\"\n");
  write_file(root / "config.toml", "default_target = \"default\"\n");

  setenv("TEST_WEBHOOK_PATH", "/env-path", 1);

  const auto loaded = manakan::load_config_tree(make_paths(root));
  manakan::CliOptions cli;
  cli.provider = "discord";
  cli.inputs["username"] = "cli-user";
  cli.positional_args.push_back("hello");

  const auto request = manakan::resolve_request(loaded, cli);
  require(request.path == "/env-path", "expected env placeholder to resolve");
  require(request.body.at("username") == "cli-user", "expected arg placeholder to resolve");
  require(request.body.at("content") == "hello", "expected argv placeholder to resolve");
}

void test_resolver_reports_missing_inputs() {
  TempDir dir;
  const auto root = dir.path / "manakan";
  write_file(root / "providers" / "discord.toml",
             "name = \"discord\"\n"
             "method = \"POST\"\n"
             "base_url = \"https://discord.com\"\n"
             "path = \"{{env.MISSING_WEBHOOK}}\"\n"
             "[body]\n"
             "username = \"{{arg.username}}\"\n"
             "content = \"{{argv.1}}\"\n");
  write_file(root / "targets" / "discord.toml",
             "use = \"discord\"\n"
             "[default]\n"
             "unused = \"ok\"\n");
  write_file(root / "config.toml", "default_provider = \"discord\"\ndefault_target = \"default\"\n");

  const auto loaded = manakan::load_config_tree(make_paths(root));
  manakan::CliOptions cli;

  try {
    (void)manakan::resolve_request(loaded, cli);
    throw std::runtime_error("expected missing input error");
  } catch (const std::runtime_error& ex) {
    const std::string message = ex.what();
    require(message.find("env.MISSING_WEBHOOK") != std::string::npos, "missing env key not reported");
    require(message.find("arg.username") != std::string::npos, "missing arg key not reported");
    require(message.find("argv.1") != std::string::npos, "missing argv key not reported");
  }
}

void test_same_target_name_requires_provider_when_ambiguous() {
  TempDir dir;
  const auto root = dir.path / "manakan";
  write_file(root / "providers" / "discord.toml",
             "name = \"discord\"\n"
             "method = \"POST\"\n"
             "base_url = \"https://discord.com\"\n"
             "path = \"/api/webhooks{{webhook_path}}\"\n"
             "[body]\n"
             "content = \"{{argv.1}}\"\n");
  write_file(root / "providers" / "chatwork.toml",
             "name = \"chatwork\"\n"
             "method = \"POST\"\n"
             "base_url = \"https://api.chatwork.com\"\n"
             "path = \"/v2/rooms/{{room_id}}/messages\"\n"
             "[body]\n"
             "body = \"{{argv.1}}\"\n");
  write_file(root / "targets" / "discord.toml",
             "use = \"discord\"\n"
             "[default]\n"
             "webhook_path = \"/abc\"\n");
  write_file(root / "targets" / "chatwork.toml",
             "use = \"chatwork\"\n"
             "[default]\n"
             "room_id = \"123\"\n");

  const auto loaded = manakan::load_config_tree(make_paths(root));
  manakan::CliOptions cli;
  cli.target = "default";
  cli.positional_args.push_back("hello");

  try {
    (void)manakan::resolve_request(loaded, cli);
    throw std::runtime_error("expected ambiguous target error");
  } catch (const std::runtime_error& ex) {
    require(std::string(ex.what()).find("exists for multiple providers") != std::string::npos,
            "expected ambiguous target error");
  }
}

void test_default_provider_and_target_are_used() {
  TempDir dir;
  const auto root = dir.path / "manakan";
  write_file(root / "providers" / "discord.toml",
             "name = \"discord\"\n"
             "method = \"POST\"\n"
             "base_url = \"https://discord.com\"\n"
             "path = \"{{webhook_path}}\"\n"
             "[body]\n"
             "content = \"{{argv.1}}\"\n");
  write_file(root / "targets" / "discord.toml",
             "use = \"discord\"\n"
             "[default]\n"
             "webhook_path = \"/default-webhook\"\n");
  write_file(root / "config.toml",
             "default_provider = \"discord\"\n"
             "default_target = \"default\"\n");

  const auto loaded = manakan::load_config_tree(make_paths(root));
  manakan::CliOptions cli;
  cli.positional_args.push_back("hello");

  const auto request = manakan::resolve_request(loaded, cli);
  require(request.provider_name == "discord", "expected default provider");
  require(request.target_name == "default", "expected default target");
  require(request.path == "/default-webhook", "expected default target path");
}

} // namespace

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
      {"config_loader_allows_same_target_name_across_providers", test_config_loader_allows_same_target_name_across_providers},
      {"resolver_prefers_cli_target_and_provider_values", test_resolver_prefers_cli_target_and_provider_values},
      {"resolver_handles_env_arg_and_argv_placeholders", test_resolver_handles_env_arg_and_argv_placeholders},
      {"resolver_reports_missing_inputs", test_resolver_reports_missing_inputs},
      {"same_target_name_requires_provider_when_ambiguous", test_same_target_name_requires_provider_when_ambiguous},
      {"default_provider_and_target_are_used", test_default_provider_and_target_are_used},
  };

  int failed = 0;
  for (const auto& test : tests) {
    try {
      test.second();
      std::cout << "[PASS] " << test.first << '\n';
    } catch (const std::exception& ex) {
      ++failed;
      std::cerr << "[FAIL] " << test.first << ": " << ex.what() << '\n';
    }
  }

  if (failed != 0) {
    std::cerr << failed << " test(s) failed" << std::endl;
    return 1;
  }

  std::cout << tests.size() << " test(s) passed" << std::endl;
  return 0;
}
