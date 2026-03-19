#pragma once

#include "rsc_test/diagnostic.hpp"

#include <string>
#include <vector>

namespace rsc
{
/// @brief Normalize output by removing carriage returns and trailing whitespace.
/// @param s Input string to normalize
/// @return Normalized string with consistent line endings and trimmed whitespace
auto normalize_output(std::string s) -> std::string;

/// @brief Check if actual stream output matches expected output specification.
/// @param label Stream label for error messages (e.g., "sctdout", "stderr")
/// @param actual Actual stream output from program execution
/// @param expected Expected stream output from program execution
/// @param exact Flag to strictly check the stream (e.g. not normalize inputs)
/// @param empty Flag to ensure the actual stream output is empty
/// @param diags Vector to store diagnostic messages for mismatches
/// @return true if output matches expectations, false otherwise
auto check_stream(const std::string &label, std::string actual, std::string expected, const bool exact,
                  const bool empty, std::vector<rsc::Diagnostic> &diags) -> bool;
} // namespace rsc
