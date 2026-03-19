#pragma once

#include "rsc_test/configuration.hpp"
#include "rsc_test/diagnostic.hpp"

#include <nlohmann/json_fwd.hpp>

#include <filesystem>

namespace rsc
{
/// @brief Execute a test program with specified inputs and capture its output.
/// @param test JSON object containing test configuration
/// @param workdir Working directory for program execution
/// @return RunResult containing captured stdout, stderr, and return code
/// @throws std::runtime_error if executable is not found or not executable
auto execute_test(const nlohmann::json &test, const std::filesystem::path &workdir) -> rsc::RunResult;

/// @brief Execute all tests according to the provided configuration options.
/// @param options Configuration options including test file, filters, and output format
auto run(const rsc::Options &options) -> void;

} // namespace rsc
