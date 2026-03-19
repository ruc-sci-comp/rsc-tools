#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <regex>
#include <set>
#include <string>

namespace rsc
{
/// @struct Options
/// @brief Command-line options and configuration for the test runner.
struct Options
{
    /// @brief Path to JSON test configuration file
    std::filesystem::path test_configuration{};

    /// @brief Regex pattern to filter test names
    std::regex filter_pattern{".*"};

    /// @brief List test names without running
    bool list_tests{};

    /// @brief Output results in JSON format
    bool json_output{};

    /// @brief Generate a new test file
    bool generate{};
};

/// @brief Parse aand validate the test configuration
/// @param test_configuration Path to the test configuration
/// @return Parsed and validated JSON configuration
/// @throws std::exception when parsing or validation fails
auto read_configuration(const std::filesystem::path &test_configuration) -> nlohmann::json;

/// @brief Generate a stubbed test configuration
/// @param test_configuration Path to save the configuration to
/// @throws std::runtime_error for a bad write or bad schema
auto generate_configuration(const std::filesystem::path &test_configuration) -> void;

/// @brief Gets the available tests from the given test configuration
/// @param filter_pattern Regex pattern to filter tests by name
/// @param tests JSON array of tests
/// @return Set of unique test names that satisfy the given filter
/// @throws std::runtime_error When tests do not have unique names
/// @throws std::regex_error When the given regex pattern is invalid
auto get_tests(const std::regex &filter_pattern, const nlohmann::json::array_t &tests) -> std::set<std::string>;

/// @brief Parse command-line arguments and return configuration options.
/// @param argc Number of command-line arguments
/// @param argv Array of command-line argument strings
/// @return Parsed Options structure containing configuration
auto cli(const int argc, const char **argv) -> Options;
} // namespace rsc