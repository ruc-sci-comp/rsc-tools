/// @file main.cpp
/// @brief Main implementation of the rsc-test tool for automated program testing.
///
/// This tool executes programs with specified inputs and validates their outputs
/// against expected results defined in JSON configuration files. It supports
/// comprehensive testing scenarios including command-line arguments, environment
/// variables, stdin input, and file system state validation.

#include "rsc_test/configuration.hpp"
#include "rsc_test/runner.hpp"

/// @brief Main entry point for the rsc-test tool.
/// @param argc Number of command-line arguments
/// @param argv Array of command-line argument strings
/// @return Exit code (0 for success, non-zero for failure)
auto main(const int argc, const char **argv) -> int
{
    auto options = rsc::cli(argc, argv);
    return rsc::run(options) ? 0 : 1;
}
