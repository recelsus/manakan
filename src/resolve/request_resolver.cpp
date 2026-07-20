#include "request_resolver.hpp"
#include "helpers.hpp"
#include "merge.hpp"
#include <cstdlib>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace manakan {
namespace {

struct ResolverContext {
  const CliOptions& cli;
  // Bare-name (no source prefix) placeholder values, collected from every scalar
  // leaf anywhere in the merged provider+target tree, then overlaid with CLI
  // --input values (highest priority). Values may themselves still contain
  // unresolved placeholders and are resolved lazily, with cycle detection.
  std::unordered_map<std::string, std::string> merged_values;
  std::unordered_map<std::string, std::string> cache;
  std::unordered_set<std::string> resolving;
  std::set<std::string> missing;
};

std::optional<std::string> get_argv_value(const CliOptions& cli, int index) {
  if (index <= 0) return std::nullopt;
  const std::size_t zero_based = static_cast<std::size_t>(index - 1);
  if (zero_based >= cli.positional_args.size()) return std::nullopt;
  return cli.positional_args[zero_based];
}

std::string join_missing(const std::set<std::string>& missing) {
  std::ostringstream oss;
  oss << "missing required inputs:";
  for (const auto& item : missing) oss << "\n  - " << item;
  return oss.str();
}

std::string resolve_template(const std::string& input, ResolverContext& ctx);

std::string resolve_named_key(const std::string& key, ResolverContext& ctx) {
  if (auto cached = ctx.cache.find(key); cached != ctx.cache.end()) return cached->second;
  if (!ctx.resolving.insert(key).second) throw std::runtime_error("cyclic placeholder reference detected: " + key);

  auto it = ctx.merged_values.find(key);
  if (it == ctx.merged_values.end()) {
    ctx.resolving.erase(key);
    ctx.missing.insert("placeholder." + key);
    return "";
  }

  const std::string resolved = resolve_template(it->second, ctx);
  ctx.resolving.erase(key);
  ctx.cache.emplace(key, resolved);
  return resolved;
}

std::string resolve_placeholder(const std::string& placeholder, ResolverContext& ctx) {
  const std::string expr = trim(placeholder);
  if (expr.rfind("env.", 0) == 0) {
    const std::string env_name = expr.substr(4);
    const char* value = std::getenv(env_name.c_str());
    if (!value) ctx.missing.insert("env." + env_name);
    return value ? std::string(value) : "";
  }

  if (expr.rfind("arg.", 0) == 0) {
    const std::string name = expr.substr(4);
    auto it = ctx.cli.inputs.find(name);
    if (it == ctx.cli.inputs.end()) {
      ctx.missing.insert("arg." + name);
      return "";
    }
    return it->second;
  }

  if (expr.rfind("argv.", 0) == 0) {
    const std::string index_str = expr.substr(5);
    try {
      const int index = std::stoi(index_str);
      auto value = get_argv_value(ctx.cli, index);
      if (!value) ctx.missing.insert("argv." + index_str);
      return value.value_or("");
    } catch (...) {
      throw std::runtime_error("invalid argv placeholder: " + expr);
    }
  }

  return resolve_named_key(expr, ctx);
}

std::string resolve_template(const std::string& input, ResolverContext& ctx) {
  static const std::regex placeholder_re(R"(\{\{([^}]+)\}\})");

  std::string output;
  std::size_t last = 0;
  for (std::sregex_iterator it(input.begin(), input.end(), placeholder_re), end; it != end; ++it) {
    const auto match = *it;
    output.append(input.substr(last, static_cast<std::size_t>(match.position()) - last));
    output.append(resolve_placeholder(match[1].str(), ctx));
    last = static_cast<std::size_t>(match.position() + match.length());
  }
  output.append(input.substr(last));
  return output;
}

// Collects every scalar leaf anywhere in the tree, keyed by its own (bare) key
// name, so `{{name}}` placeholders can reference a value defined at any depth in
// any top-level section -- not just the section it is used from.
void collect_named_values(const TomlValue& value, std::unordered_map<std::string, std::string>& out) {
  if (!value.is_table()) return;
  for (const auto& kv : value.table()) {
    if (kv.second.is_scalar()) out[kv.first] = kv.second.scalar();
    collect_named_values(kv.second, out);
  }
}

// Recursively resolves placeholders over a merged tree. At every table level, a
// key whose bare name matches a CLI --input overrides that entire subtree with
// the literal CLI value, bypassing whatever template/value the config declared.
TomlValue resolve_tree(const TomlValue& value, ResolverContext& ctx) {
  if (value.is_scalar()) return TomlValue(resolve_template(value.scalar(), ctx));

  if (value.is_array()) {
    TomlValue::Array out;
    out.reserve(value.array().size());
    for (const auto& item : value.array()) out.push_back(resolve_tree(item, ctx));
    return TomlValue(std::move(out));
  }

  TomlValue::Table out;
  for (const auto& kv : value.table()) {
    auto cli_override = ctx.cli.inputs.find(kv.first);
    if (cli_override != ctx.cli.inputs.end()) {
      out.emplace(kv.first, TomlValue(cli_override->second));
      continue;
    }
    out.emplace(kv.first, resolve_tree(kv.second, ctx));
  }
  return TomlValue(std::move(out));
}

std::string select_provider_name(const LoadedConfig& config, const CliOptions& cli) {
  if (cli.provider) return *cli.provider;
  if (config.defaults.default_provider) return *config.defaults.default_provider;
  throw std::runtime_error("provider is required; use --provider or configure default_provider");
}

const TargetConfig& select_target(const LoadedConfig& config, const std::string& provider_name, const CliOptions& cli) {
  std::vector<const TargetConfig*> candidates;
  for (const auto& target : config.targets) {
    if (target.use == provider_name) candidates.push_back(&target);
  }
  if (candidates.empty()) throw std::runtime_error("no targets found for provider: " + provider_name);

  if (cli.target) {
    for (const auto* target : candidates) {
      if (target->name == *cli.target) return *target;
    }
    throw std::runtime_error("target '" + *cli.target + "' not found for provider '" + provider_name + "'");
  }

  for (const auto* target : candidates) {
    if (target->is_default) return *target;
  }

  if (candidates.size() == 1) return *candidates.front();

  throw std::runtime_error("no target specified and provider '" + provider_name +
                            "' has no default target; use --target");
}

} // namespace

ResolvedRequest resolve_request(const LoadedConfig& config, const CliOptions& cli) {
  const std::string provider_name = select_provider_name(config, cli);
  auto provider_it = config.providers.find(provider_name);
  if (provider_it == config.providers.end()) throw std::runtime_error("provider not found: " + provider_name);
  const ProviderConfig& provider = provider_it->second;

  const TargetConfig& target = select_target(config, provider_name, cli);

  const TomlValue merged = merge_provider_and_target(provider, target);

  ResolverContext ctx{cli, {}, {}, {}, {}};
  collect_named_values(merged, ctx.merged_values);

  const TomlValue resolved = resolve_tree(merged, ctx);
  if (!ctx.missing.empty()) throw std::runtime_error(join_missing(ctx.missing));

  const TomlValue* request_section = resolved.find("request");
  if (!request_section || !request_section->is_table()) {
    throw std::runtime_error("internal error: resolved tree missing [request] section");
  }
  auto scalar_or = [&](const char* key, const std::string& fallback) -> std::string {
    const TomlValue* value = request_section->find(key);
    return (value && value->is_scalar()) ? value->scalar() : fallback;
  };

  ResolvedRequest request;
  request.provider_name = provider_name;
  request.target_name = target.name;
  request.method = scalar_or("method", "");
  request.base_url = scalar_or("base_url", "");
  request.path = scalar_or("path", "/");

  if (const TomlValue* headers = resolved.find("headers"); headers && headers->is_table()) {
    for (const auto& kv : headers->table()) {
      if (!kv.second.is_scalar()) throw std::runtime_error("header '" + kv.first + "' must resolve to a scalar value");
      request.headers[kv.first] = kv.second.scalar();
    }
  }

  if (const TomlValue* body = resolved.find("data")) {
    request.body = *body;
  } else {
    request.body = TomlValue(TomlValue::Table{});
  }

  return request;
}

} // namespace manakan
