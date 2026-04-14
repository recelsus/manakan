#pragma once

#include "models.hpp"

namespace manakan {

ResolvedRequest resolve_request(const LoadedConfig& config, const CliOptions& cli);

} // namespace manakan
