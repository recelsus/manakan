#pragma once

#include "models.hpp"

namespace manakan {

// Recursively merges target.tree into provider.tree following the table/scalar/array
// merge rules and const/c protection described in
// reference/manakan_toml.requirements.md:
//   - table + table merges recursively
//   - scalar/array, or a type mismatch, means the Target's value replaces wholesale
//   - a Target write into a path covered by provider.const_paths (exactly, an
//     ancestor of it, or a descendant of it) is rejected
// Throws std::runtime_error on a const violation.
TomlValue merge_provider_and_target(const ProviderConfig& provider, const TargetConfig& target);

} // namespace manakan
