#include "merge.hpp"
#include <stdexcept>

namespace manakan {
namespace {

// True if `path` is exactly one of `const_paths`, a descendant of one (path is
// nested under it), or an ancestor of one (path's subtree contains it). All three
// cases must be rejected: the first two are direct edits to protected data, the
// third would destroy a protected descendant by replacing its parent wholesale.
bool touches_const_path(const std::string& path, const std::vector<std::string>& const_paths) {
  for (const auto& const_path : const_paths) {
    if (path == const_path) return true;
    if (path.size() > const_path.size() && path.compare(0, const_path.size(), const_path) == 0 &&
        path[const_path.size()] == '.') {
      return true; // path is a descendant of const_path
    }
    if (const_path.size() > path.size() && const_path.compare(0, path.size(), path) == 0 &&
        const_path[path.size()] == '.') {
      return true; // path is an ancestor of const_path
    }
  }
  return false;
}

TomlValue merge_value(const TomlValue& provider_value, const TomlValue& target_value, const std::string& path,
                       const std::string& provider_name, const std::vector<std::string>& const_paths) {
  if (provider_value.is_table() && target_value.is_table()) {
    TomlValue::Table merged = provider_value.table();
    for (const auto& kv : target_value.table()) {
      const std::string child_path = path.empty() ? kv.first : path + "." + kv.first;
      auto it = merged.find(kv.first);
      if (it != merged.end()) {
        merged[kv.first] = merge_value(it->second, kv.second, child_path, provider_name, const_paths);
      } else {
        if (touches_const_path(child_path, const_paths)) {
          throw std::runtime_error("target attempts to write to const-protected path '" + child_path +
                                    "' of provider '" + provider_name + "'");
        }
        merged.emplace(kv.first, kv.second);
      }
    }
    return TomlValue(std::move(merged));
  }

  // Not table+table: the Target replaces this path's value wholesale (scalar,
  // array, or a type change against the Provider's value).
  if (touches_const_path(path, const_paths)) {
    throw std::runtime_error("target attempts to override const-protected path '" + path + "' of provider '" +
                              provider_name + "'");
  }
  return target_value;
}

} // namespace

TomlValue merge_provider_and_target(const ProviderConfig& provider, const TargetConfig& target) {
  return merge_value(provider.tree, target.tree, "", provider.name, provider.const_paths);
}

} // namespace manakan
