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

std::string data_field(const manakan::ResolvedRequest& request, const std::string& key) {
  const auto* value = request.body.find(key);
  require(value != nullptr, "expected data field: " + key);
  return value->scalar();
}

void write_discord_provider(const fs::path& root) {
  write_file(root / "providers" / "discord.toml",
             "name = \"discord\"\n"
             "[request]\n"
             "method = \"POST\"\n"
             "base_url = \"https://discord.com\"\n"
             "path = \"/api/webhooks{{webhook_path}}\"\n"
             "[data]\n"
             "content = \"{{argv.1}}\"\n");
}

void write_chatwork_provider(const fs::path& root) {
  write_file(root / "providers" / "chatwork.toml",
             "name = \"chatwork\"\n"
             "[request]\n"
             "method = \"POST\"\n"
             "base_url = \"https://api.chatwork.com\"\n"
             "path = \"/v2/rooms/{{room_id}}/messages\"\n"
             "[data]\n"
             "body = \"{{argv.1}}\"\n");
}

void test_config_loader_allows_same_target_name_across_providers() {
  TempDir dir;
  const auto root = dir.path / "manakan";
  write_discord_provider(root);
  write_chatwork_provider(root);
  write_file(root / "targets" / "discord.toml",
             "use = \"discord\"\n"
             "[main]\n"
             "data.webhook_path = \"/abc\"\n");
  write_file(root / "targets" / "chatwork.toml",
             "use = \"chatwork\"\n"
             "[main]\n"
             "data.room_id = \"123\"\n");

  const auto loaded = manakan::load_config_tree(make_paths(root));
  require(loaded.warnings.empty(), "expected no warnings, got: " + (loaded.warnings.empty() ? std::string() : loaded.warnings.front()));

  int matching = 0;
  for (const auto& target : loaded.targets) {
    if (target.name == "main") ++matching;
  }
  require(matching == 2, "expected two targets named 'main', one per provider");
}

void test_resolver_prefers_cli_target_and_provider_values() {
  TempDir dir;
  const auto root = dir.path / "manakan";
  write_file(root / "providers" / "discord.toml",
             "name = \"discord\"\n"
             "[request]\n"
             "method = \"POST\"\n"
             "base_url = \"https://discord.com\"\n"
             "path = \"{{webhook_path}}\"\n"
             "[data]\n"
             "username = \"{{username}}\"\n"
             "content = \"{{argv.1}}\"\n");
  write_file(root / "targets" / "discord.toml",
             "use = \"discord\"\n"
             "default = \"main\"\n"
             "[main]\n"
             "data.webhook_path = \"/target\"\n"
             "data.username = \"target-user\"\n");
  write_file(root / "config.toml", "default_provider = \"discord\"\n");

  const auto loaded = manakan::load_config_tree(make_paths(root));
  manakan::CliOptions cli;
  cli.inputs["username"] = "cli-user";
  cli.inputs["content"] = "cli-content";
  cli.positional_args.push_back("argv-content");

  const auto request = manakan::resolve_request(loaded, cli);
  require(request.provider_name == "discord", "expected discord provider");
  require(request.target_name == "main", "expected per-provider default target 'main'");
  require(request.path == "/target", "target should override provider value");
  require(data_field(request, "username") == "cli-user", "--input should override target value");
  require(data_field(request, "content") == "cli-content", "--input should override argv");
}

void test_resolver_handles_env_arg_and_argv_placeholders() {
  TempDir dir;
  const auto root = dir.path / "manakan";
  write_file(root / "providers" / "discord.toml",
             "name = \"discord\"\n"
             "[request]\n"
             "method = \"POST\"\n"
             "base_url = \"https://discord.com\"\n"
             "path = \"{{env.TEST_WEBHOOK_PATH}}\"\n"
             "[data]\n"
             "username = \"{{arg.username}}\"\n"
             "content = \"{{argv.1}}\"\n");
  write_file(root / "targets" / "discord.toml",
             "use = \"discord\"\n"
             "default = \"main\"\n"
             "[main]\n"
             "data.unused = \"ok\"\n");

  setenv("TEST_WEBHOOK_PATH", "/env-path", 1);

  const auto loaded = manakan::load_config_tree(make_paths(root));
  manakan::CliOptions cli;
  cli.provider = "discord";
  cli.inputs["username"] = "cli-user";
  cli.positional_args.push_back("hello");

  const auto request = manakan::resolve_request(loaded, cli);
  require(request.path == "/env-path", "expected env placeholder to resolve");
  require(data_field(request, "username") == "cli-user", "expected arg placeholder to resolve");
  require(data_field(request, "content") == "hello", "expected argv placeholder to resolve");
}

void test_resolver_reports_missing_inputs() {
  TempDir dir;
  const auto root = dir.path / "manakan";
  write_file(root / "providers" / "discord.toml",
             "name = \"discord\"\n"
             "[request]\n"
             "method = \"POST\"\n"
             "base_url = \"https://discord.com\"\n"
             "path = \"{{env.MISSING_WEBHOOK}}\"\n"
             "[data]\n"
             "username = \"{{arg.username}}\"\n"
             "content = \"{{argv.1}}\"\n");
  write_file(root / "targets" / "discord.toml",
             "use = \"discord\"\n"
             "default = \"main\"\n"
             "[main]\n"
             "data.unused = \"ok\"\n");
  write_file(root / "config.toml", "default_provider = \"discord\"\n");

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

void test_provider_is_required_regardless_of_target_uniqueness() {
  // Provider selection no longer derives from target ambiguity: with no --provider
  // and no default_provider configured, resolution must fail even if every target
  // name happens to be unique across providers.
  TempDir dir;
  const auto root = dir.path / "manakan";
  write_discord_provider(root);
  write_chatwork_provider(root);
  write_file(root / "targets" / "discord.toml",
             "use = \"discord\"\n"
             "[alerts]\n"
             "data.webhook_path = \"/abc\"\n");
  write_file(root / "targets" / "chatwork.toml",
             "use = \"chatwork\"\n"
             "[ops]\n"
             "data.room_id = \"123\"\n");

  const auto loaded = manakan::load_config_tree(make_paths(root));
  manakan::CliOptions cli;
  cli.target = "alerts";
  cli.positional_args.push_back("hello");

  try {
    (void)manakan::resolve_request(loaded, cli);
    throw std::runtime_error("expected provider-required error");
  } catch (const std::runtime_error& ex) {
    require(std::string(ex.what()).find("provider is required") != std::string::npos,
            "expected provider-required error, got: " + std::string(ex.what()));
  }
}

void test_default_provider_and_target_are_used() {
  TempDir dir;
  const auto root = dir.path / "manakan";
  write_file(root / "providers" / "discord.toml",
             "name = \"discord\"\n"
             "[request]\n"
             "method = \"POST\"\n"
             "base_url = \"https://discord.com\"\n"
             "path = \"{{webhook_path}}\"\n"
             "[data]\n"
             "content = \"{{argv.1}}\"\n");
  write_file(root / "targets" / "discord.toml",
             "use = \"discord\"\n"
             "default = \"main\"\n"
             "[main]\n"
             "data.webhook_path = \"/default-webhook\"\n");
  write_file(root / "config.toml", "default_provider = \"discord\"\n");

  const auto loaded = manakan::load_config_tree(make_paths(root));
  manakan::CliOptions cli;
  cli.positional_args.push_back("hello");

  const auto request = manakan::resolve_request(loaded, cli);
  require(request.provider_name == "discord", "expected default provider");
  require(request.target_name == "main", "expected per-provider default target");
  require(request.path == "/default-webhook", "expected default target path");
}

void test_const_protects_field_from_target_override() {
  TempDir dir;
  const auto root = dir.path / "manakan";
  write_file(root / "providers" / "discord.toml",
             "name = \"discord\"\n"
             "[request]\n"
             "method = \"POST\"\n"
             "base_url = \"https://discord.com\"\n"
             "path = \"/webhook\"\n"
             "[request.const]\n"
             "content_type_locked = \"application/json\"\n"
             "[data]\n"
             "content = \"{{argv.1}}\"\n");
  write_file(root / "targets" / "discord.toml",
             "use = \"discord\"\n"
             "default = \"main\"\n"
             "[main]\n"
             "request.content_type_locked = \"text/plain\"\n");

  const auto loaded = manakan::load_config_tree(make_paths(root));
  manakan::CliOptions cli;
  cli.provider = "discord";
  cli.positional_args.push_back("hello");

  try {
    (void)manakan::resolve_request(loaded, cli);
    throw std::runtime_error("expected const violation error");
  } catch (const std::runtime_error& ex) {
    require(std::string(ex.what()).find("const-protected") != std::string::npos,
            "expected const-protected error, got: " + std::string(ex.what()));
  }
}

void test_const_allows_sibling_override_partial_protection() {
  // Mirrors reference/manakan_toml.requirements.md's partial-fixation example:
  // a const-protected field and a mutable sibling field under the same table.
  TempDir dir;
  const auto root = dir.path / "manakan";
  write_file(root / "providers" / "discord.toml",
             "name = \"discord\"\n"
             "[request]\n"
             "method = \"POST\"\n"
             "base_url = \"https://discord.com\"\n"
             "path = \"/webhook\"\n"
             "[data]\n"
             "user.id = \"1\"\n"
             "[data.const]\n"
             "user.token = \"secret\"\n");
  write_file(root / "targets" / "discord.toml",
             "use = \"discord\"\n"
             "default = \"main\"\n"
             "[main]\n"
             "data.user.id = \"2\"\n");

  const auto loaded = manakan::load_config_tree(make_paths(root));
  const auto& provider = loaded.providers.at("discord");
  bool protects_token = false;
  for (const auto& path : provider.const_paths) {
    if (path == "data.user.token") protects_token = true;
  }
  require(protects_token, "expected data.user.token to be recorded as const-protected");

  manakan::CliOptions cli;
  cli.provider = "discord";
  cli.positional_args.push_back("hello");
  const auto request = manakan::resolve_request(loaded, cli);
  const auto* user = request.body.find("user");
  require(user != nullptr && user->is_table(), "expected data.user to be a table");
  require(user->find("id")->scalar() == "2", "expected target override of non-const sibling id");
  require(user->find("token")->scalar() == "secret", "expected const-protected token to survive unchanged");
}

void test_target_const_and_c_keys_are_ordinary_data() {
  // `const`/`c` are only special when they appear in a Provider file, where they
  // mark a subtree as protected (see test_const_protects_field_from_target_override
  // and test_const_allows_sibling_override_partial_protection). A Target file may
  // use the literal keys "const"/"c" too, but per
  // reference/manakan_toml.requirements.md section 10 ("constの予約") they are not
  // reserved there: no promotion happens, nothing gets protected, and they just
  // become ordinary nested fields under whichever section the Target wrote them
  // into. This test uses the exact same [data.const]-shaped nesting a Provider
  // would use for protection, but authored on the Target side, to prove it is
  // treated as plain data instead.
  TempDir dir;
  const auto root = dir.path / "manakan";
  write_file(root / "providers" / "discord.toml",
             "name = \"discord\"\n"
             "[request]\n"
             "method = \"POST\"\n"
             "base_url = \"https://discord.com\"\n"
             "path = \"/webhook\"\n"
             "[data]\n"
             "content = \"{{argv.1}}\"\n");
  write_file(root / "targets" / "discord.toml",
             "use = \"discord\"\n"
             "default = \"main\"\n"
             "[main]\n"
             "data.const.locked = \"target-const-value\"\n"
             "data.c.also_locked = \"target-c-value\"\n");

  const auto loaded = manakan::load_config_tree(make_paths(root));
  const auto& provider = loaded.providers.at("discord");
  require(provider.const_paths.empty(),
          "provider declared no const/c block; nothing should be registered as const-protected");

  manakan::CliOptions cli;
  cli.provider = "discord";
  cli.positional_args.push_back("hello");
  const auto request = manakan::resolve_request(loaded, cli);

  // "const" and "c" survive as literal, unpromoted nested keys under `data`,
  // exactly where the Target put them -- not hoisted up to become siblings of
  // `content` the way a Provider-side const/c block would be.
  const auto* const_field = request.body.find("const");
  require(const_field != nullptr && const_field->is_table(), "expected 'const' to remain a literal nested table under data");
  require(const_field->find("locked")->scalar() == "target-const-value", "expected data.const.locked to survive as ordinary data");

  const auto* c_field = request.body.find("c");
  require(c_field != nullptr && c_field->is_table(), "expected 'c' to remain a literal nested table under data");
  require(c_field->find("also_locked")->scalar() == "target-c-value", "expected data.c.also_locked to survive as ordinary data");

  require(request.body.find("locked") == nullptr, "'const' contents must not be promoted up to data's top level");
  require(request.body.find("also_locked") == nullptr, "'c' contents must not be promoted up to data's top level");
}

void test_provider_name_does_not_leak_as_named_placeholder() {
  // The Provider's own `name` field is identity metadata, not user data. It must
  // not be reachable via a bare {{name}} placeholder just because it happens to
  // sit at the root of the tree collect_named_values walks.
  TempDir dir;
  const auto root = dir.path / "manakan";
  write_file(root / "providers" / "leaktest.toml",
             "name = \"leaktest\"\n"
             "[request]\n"
             "method = \"POST\"\n"
             "base_url = \"https://example.com\"\n"
             "path = \"/x\"\n"
             "[data]\n"
             "whoami = \"{{name}}\"\n");
  write_file(root / "targets" / "leaktest.toml",
             "use = \"leaktest\"\n"
             "default = \"main\"\n"
             "[main]\n"
             "data.dummy = \"1\"\n");

  const auto loaded = manakan::load_config_tree(make_paths(root));
  manakan::CliOptions cli;
  cli.provider = "leaktest";
  cli.positional_args.push_back("hello");

  try {
    (void)manakan::resolve_request(loaded, cli);
    throw std::runtime_error("expected missing placeholder error for {{name}}");
  } catch (const std::runtime_error& ex) {
    require(std::string(ex.what()).find("placeholder.name") != std::string::npos,
            "expected {{name}} to be reported missing, not resolved to the provider's own name; got: " +
                std::string(ex.what()));
  }
}

void test_array_placeholders_and_wholesale_array_replacement() {
  // reference/manakan_toml.requirements.md sections 5/12: arrays are one value
  // (Target replaces wholesale, never merged element-by-element), but string
  // elements inside an array still get placeholder substitution.
  TempDir dir;
  const auto root = dir.path / "manakan";
  write_file(root / "providers" / "discord.toml",
             "name = \"discord\"\n"
             "[request]\n"
             "method = \"POST\"\n"
             "base_url = \"https://discord.com\"\n"
             "path = \"/webhook\"\n"
             "[data]\n"
             "content = \"{{argv.1}}\"\n"
             "tags = [\"a\", \"prefix-{{arg.suffix}}\"]\n");
  write_file(root / "targets" / "discord.toml",
             "use = \"discord\"\n"
             "[keep]\n"
             "data.username = \"x\"\n"
             "[replace]\n"
             "data.tags = \"not-an-array-anymore\"\n");

  const auto loaded = manakan::load_config_tree(make_paths(root));

  {
    manakan::CliOptions cli;
    cli.provider = "discord";
    cli.target = "keep";
    cli.inputs["suffix"] = "hello";
    cli.positional_args.push_back("msg");
    const auto request = manakan::resolve_request(loaded, cli);
    const auto* tags = request.body.find("tags");
    require(tags != nullptr && tags->is_array(), "expected tags to remain an array when target doesn't touch it");
    require(tags->array().size() == 2, "expected two array elements");
    require(tags->array()[0].scalar() == "a", "expected first array element unchanged");
    require(tags->array()[1].scalar() == "prefix-hello", "expected placeholder inside array element to resolve");
  }

  {
    manakan::CliOptions cli;
    cli.provider = "discord";
    cli.target = "replace";
    cli.positional_args.push_back("msg");
    const auto request = manakan::resolve_request(loaded, cli);
    const auto* tags = request.body.find("tags");
    require(tags != nullptr && tags->is_scalar(), "expected target to wholesale-replace the array with a scalar");
    require(tags->scalar() == "not-an-array-anymore", "expected replaced scalar value");
  }
}

} // namespace

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
      {"config_loader_allows_same_target_name_across_providers", test_config_loader_allows_same_target_name_across_providers},
      {"resolver_prefers_cli_target_and_provider_values", test_resolver_prefers_cli_target_and_provider_values},
      {"resolver_handles_env_arg_and_argv_placeholders", test_resolver_handles_env_arg_and_argv_placeholders},
      {"resolver_reports_missing_inputs", test_resolver_reports_missing_inputs},
      {"provider_is_required_regardless_of_target_uniqueness", test_provider_is_required_regardless_of_target_uniqueness},
      {"default_provider_and_target_are_used", test_default_provider_and_target_are_used},
      {"const_protects_field_from_target_override", test_const_protects_field_from_target_override},
      {"const_allows_sibling_override_partial_protection", test_const_allows_sibling_override_partial_protection},
      {"target_const_and_c_keys_are_ordinary_data", test_target_const_and_c_keys_are_ordinary_data},
      {"provider_name_does_not_leak_as_named_placeholder", test_provider_name_does_not_leak_as_named_placeholder},
      {"array_placeholders_and_wholesale_array_replacement", test_array_placeholders_and_wholesale_array_replacement},
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
