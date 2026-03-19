#pragma once

#include "rsc_test/diagnostic.hpp"

#include <filesystem>
#include <optional>

namespace rsc
{
/// @brief Execute the Lua validation script on the test context
/// @param script Path to the validation script
/// @param result Test context
/// @param workdir Test location
/// @return A diagnostic if the validation failed, otherwise nullopt
/// @throws std::runtime_error for malformed Lua script or script structure
auto run_validation_script(const std::filesystem::path &script, const rsc::RunResult &result,
                           const std::filesystem::path &workdir) -> std::optional<rsc::Diagnostic>;
} // namespace rsc