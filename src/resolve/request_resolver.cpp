#include "request_resolver.hpp"
#include "helpers.hpp"
#include <algorithm>
#include <cstdlib>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace manakan {
namespace {

struct ResolverContext {
  const ProviderConfig& provider;
  const TargetConfig& target;
  const CliOptions& cli;
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

const TargetConfig& select_target(const LoadedConfig& config, const CliOptions& cli) {
  std::optional<std::string> name = cli.target;
  if (!name) name = config.defaults.default_target;
  if (!name && config.targets_by_name.size() == 1 && config.targets_by_name.begin()->second.size() == 1) {
    name = config.targets_by_name.begin()->first;
  }
  if (!name) throw std::runtime_error("target is required; use --target or configure default_target");

  auto it = config.targets_by_name.find(*name);
  if (it == config.targets_by_name.end()) throw std::runtime_error("target not found: " + *name);

  const auto& candidates = it->second;
  if (candidates.size() == 1) return candidates.front();

  std::optional<std::string> provider_name = cli.provider;
  if (!provider_name) provider_name = config.defaults.default_provider;
  if (!provider_name) {
    throw std::runtime_error(
        "target '" + *name + "' exists for multiple providers; specify --provider or configure default_provider");
  }

  auto match = std::find_if(candidates.begin(), candidates.end(), [&](const TargetConfig& target) {
    return target.use == *provider_name;
  });
  if (match == candidates.end()) {
    throw std::runtime_error("target '" + *name + "' does not exist for provider '" + *provider_name + "'");
  }
  return *match;
}

const ProviderConfig& select_provider(const LoadedConfig& config, const CliOptions& cli, const TargetConfig& target) {
  std::string name = target.use;
  if (cli.provider && *cli.provider != name) {
    throw std::runtime_error("provider mismatch: target '" + target.name + "' uses '" + name + "', but CLI requested '" + *cli.provider + "'");
  }

  if (cli.provider) name = *cli.provider;
  else if (name.empty() && config.defaults.default_provider) name = *config.defaults.default_provider;

  auto it = config.providers.find(name);
  if (it == config.providers.end()) throw std::runtime_error("provider not found: " + name);
  return it->second;
}

std::unordered_map<std::string, std::string> resolve_map(const std::unordered_map<std::string, std::string>& input, ResolverContext& ctx) {
  std::unordered_map<std::string, std::string> out;
  for (const auto& kv : input) {
    auto cli_override = ctx.cli.inputs.find(kv.first);
    if (cli_override != ctx.cli.inputs.end()) {
      out.emplace(kv.first, cli_override->second);
      continue;
    }
    out.emplace(kv.first, resolve_template(kv.second, ctx));
  }
  return out;
}

} // namespace

ResolvedRequest resolve_request(const LoadedConfig& config, const CliOptions& cli) {
  const auto& target = select_target(config, cli);
  const auto& provider = select_provider(config, cli, target);

  ResolverContext ctx{provider, target, cli, {}, {}, {}, {}};

  for (const auto& kv : provider.values) ctx.merged_values[kv.first] = kv.second;
  for (const auto& kv : target.values) ctx.merged_values[kv.first] = kv.second;
  for (const auto& kv : cli.inputs) ctx.merged_values[kv.first] = kv.second;

  ResolvedRequest request;
  request.provider_name = provider.name;
  request.target_name = target.name;
  request.method = resolve_template(provider.method, ctx);
  request.base_url = resolve_template(provider.base_url, ctx);
  request.path = resolve_template(provider.path, ctx);
  request.headers = resolve_map(provider.headers, ctx);
  request.body = resolve_map(provider.body, ctx);

  if (!ctx.missing.empty()) throw std::runtime_error(join_missing(ctx.missing));
  return request;
}

} // namespace manakan
