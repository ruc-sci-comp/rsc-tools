#pragma once

#include <string>

namespace rsc
{
/// @struct Diagnostic
/// @brief Represents a test failure diagnostic message.
struct Diagnostic
{
    /// @brief Human-readable diagnostic message
    std::string message{};
};

/// @struct RunResult
/// @brief Captures the result of executing a test program.
struct RunResult
{
    /// @brief Captured stdout output
    std::string stdout_text{};

    /// @brief Captured stderr output
    std::string stderr_text{};

    /// @brief Program exit code
    int returncode{};
};
} // namespace rsc
