/// @file main.cpp
/// @brief Main implementation of the rsc-test tool for automated program testing.
///
/// This tool executes programs with specified inputs and validates their outputs
/// against expected results defined in JSON configuration files. It supports
/// comprehensive testing scenarios including command-line arguments, environment
/// variables, stdin input, and file system state validation.

#include <CLI/CLI.hpp>
#include <cpp-subprocess/subprocess.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <regex>
#include <string>
#include <vector>

/// @struct Options
/// @brief Command-line options and configuration for the test runner.
struct Options
{
    /// @brief Path to JSON test configuration file
    std::filesystem::path test_configuration{};

    /// @brief Regex pattern to filter test names
    std::string filter_pattern{};

    /// @brief List test names without running
    bool list_tests{};

    /// @brief Output results in JSON format
    bool json_output{};
};

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

/// @brief Parse command-line arguments and return configuration options.
/// @param argc Number of command-line arguments
/// @param argv Array of command-line argument strings
/// @return Parsed Options structure containing configuration
auto cli(const int argc, const char **argv) -> Options
{
    auto app = CLI::App{"rsc-test"};
    auto options = Options{};
    app.add_option("test_configuration", options.test_configuration)->required();
    app.add_option("--filter", options.filter_pattern, "Filter test names using regex pattern");
    app.add_flag("--list", options.list_tests, "List test names without running them");
    app.add_flag("--json", options.json_output, "Output results as JSON");
    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError &e)
    {
        std::exit(app.exit(e));
    }
    return options;
}

/// @brief Normalize output by removing carriage returns and trailing whitespace.
/// @param s Input string to normalize
/// @return Normalized string with consistent line endings and trimmed whitespace
auto normalize_output(std::string s) -> std::string
{
    auto it = std::remove(s.begin(), s.end(), '\r');
    s.erase(it, s.end());

    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    {
        s.pop_back();
    }

    return s;
}

/// @brief Check if actual stream output matches expected output specification.
/// @param label Stream label for error messages (e.g., "stdout", "stderr")
/// @param actual Actual stream output from program execution
/// @param expected JSON object containing expected output specification
/// @param diags Vector to store diagnostic messages for mismatches
/// @return true if output matches expectations, false otherwise
auto check_stream(const std::string &label, const std::string &actual, const nlohmann::json &expected,
                  std::vector<Diagnostic> &diags) -> bool
{
    auto exact = expected.value("exact", false);

    auto act = actual;
    auto exp = expected.value("text", std::string{});

    if (!exact)
    {
        act = normalize_output(act);
        exp = normalize_output(exp);
    }

    if (expected.value("empty", false))
    {
        if (!act.empty())
        {
            diags.push_back({std::format("{} expected empty, received {} bytes", label, act.size())});
            return false;
        }
        return true;
    }

    if (expected.contains("text"))
    {
        if (act != exp)
        {
            diags.push_back({std::format("{} mismatch:\n"
                                         "    expected: \"{}\"\n"
                                         "    received: \"{}\"",
                                         label, exp, act)});
            return false;
        }
    }

    return true;
}

/// @brief Execute a test program with specified inputs and capture its output.
/// @param test JSON object containing test configuration
/// @param workdir Working directory for program execution
/// @return RunResult containing captured stdout, stderr, and return code
/// @throws std::runtime_error if executable is not found or not executable
auto run_subprocess(const nlohmann::json &test, const std::filesystem::path &workdir) -> RunResult
{
    auto executable = test.at("executable").get<std::filesystem::path>();

    auto cmd = std::vector<std::string>{executable.string()};

    auto input_cfg = test.value("input", nlohmann::json::object());
    cmd.append_range(input_cfg.value("argv", std::vector<std::string>{}));

    auto env_map = input_cfg.value("env", subprocess::env_map_t{});

    auto stdin_data = input_cfg.value("stdin", std::vector<std::string>{}) | std::views::join_with(std::string("\n")) |
                      std::ranges::to<std::string>();

    // Ensure stdin data ends with newline to prevent hanging
    if (!stdin_data.empty() && stdin_data.back() != '\n')
    {
        stdin_data += '\n';
    }

    try
    {
        spdlog::info("Executing: {} in directory: {}", executable.string(), workdir.string());

        auto p = subprocess::Popen{cmd,
                                   subprocess::cwd{workdir.string()},
                                   subprocess::environment{env_map},
                                   subprocess::output{subprocess::PIPE},
                                   subprocess::error{subprocess::PIPE},
                                   subprocess::input{subprocess::PIPE}};

        auto result = p.communicate(stdin_data);

        return RunResult{.stdout_text = std::string({result.first.buf.data(), result.first.length}),
                         .stderr_text = std::string({result.second.buf.data(), result.second.length}),
                         .returncode = p.retcode()};
    }
    catch (const subprocess::CalledProcessError &e)
    {
        spdlog::error("ERROR: subprocess::CalledProcessError occurred");
        spdlog::error("       Command: {}", executable.string());
        spdlog::error("       Working directory: {}", workdir.string());
        spdlog::error("       Error: {}", e.what());
        throw;
    }
    catch (const std::exception &e)
    {
        spdlog::error("ERROR: Failed to execute '{}'", executable.string());
        spdlog::error("       Working directory: {}", workdir.string());
        spdlog::error("       Error: {}", e.what());
        throw;
    }
}

/// @brief Execute all tests according to the provided configuration options.
/// @param options Configuration options including test file, filters, and output format
auto run(const Options &options) -> void
{
    auto itc = std::ifstream{options.test_configuration};
    auto config = nlohmann::json::parse(itc);

    // Handle --list option
    if (options.list_tests)
    {
        for (const auto &test : config["tests"])
        {
            auto test_name = test.value("name", "<unnamed>");

            // Apply filter if specified
            if (!options.filter_pattern.empty())
            {
                try
                {
                    auto regex = std::regex{options.filter_pattern};
                    if (!std::regex_search(test_name, regex))
                    {
                        continue;
                    }
                }
                catch (const std::regex_error &e)
                {
                    spdlog::error("Invalid regex pattern: {}", e.what());
                    return;
                }
            }

            spdlog::info("{}", test_name);
        }
        return;
    }

    // Prepare JSON output if requested
    auto json_results = nlohmann::json::array();

    for (const auto &test : config["tests"])
    {
        auto test_name = test.value("name", "<unnamed>");

        // Apply filter if specified
        if (!options.filter_pattern.empty())
        {
            try
            {
                auto regex = std::regex{options.filter_pattern};
                if (!std::regex_search(test_name, regex))
                {
                    continue;
                }
            }
            catch (const std::regex_error &e)
            {
                spdlog::error("Invalid regex pattern: {}", e.what());
                return;
            }
        }

        if (!options.json_output)
        {
            spdlog::info("Running test: {}", test_name);
        }

        auto workdir = std::filesystem::temp_directory_path() / "rsc-test";

        std::filesystem::remove_all(workdir);
        std::filesystem::create_directories(workdir);

        // Create symlink to executable in the test working directory
        auto executable = test.at("executable").get<std::filesystem::path>();
        if (!executable.is_absolute())
        {
            auto abs_executable = std::filesystem::canonical(options.test_configuration.parent_path() / executable);
            // Create symlink with the original executable name in the working directory, preserving any nested
            // directories
            auto symlink_path = std::filesystem::weakly_canonical(workdir / executable);
            std::filesystem::create_directories(symlink_path.parent_path());
            try
            {
                std::filesystem::create_symlink(abs_executable, symlink_path);
            }
            catch (const std::filesystem::filesystem_error &e)
            {
                spdlog::warn("Failed to create symlink {} -> {}: {}", symlink_path.string(), abs_executable.string(),
                             e.what());
            }
        }

        // Handle resources - support both old format and new simplified format
        auto resources = test.value("resources", nlohmann::json::array());
        for (const auto &r : resources)
        {
            if (r.is_string())
            {
                auto src_path = r.get<std::string>();
                std::filesystem::copy(src_path, workdir / src_path, std::filesystem::copy_options::recursive);
            }
            else if (r.is_object())
            {
                std::filesystem::copy(r.at("src").get<std::string>(), workdir / r.at("dst").get<std::string>(),
                                      std::filesystem::copy_options::recursive);
            }
        }

        auto result = run_subprocess(test, workdir);

        auto ok = true;
        auto diags = std::vector<Diagnostic>{};

        auto expected_rc = test.value("returncode", 0);
        if (result.returncode != expected_rc)
        {
            diags.push_back({std::format("return code mismatch:\n"
                                         "    expected: {}\n"
                                         "    received: {}",
                                         expected_rc, result.returncode)});
            ok = false;
        }

        auto output = test.value("output", nlohmann::json::object());

        if (output.contains("stdout"))
        {
            ok &= check_stream("stdout", result.stdout_text, output["stdout"], diags);
        }

        if (output.contains("stderr"))
        {
            ok &= check_stream("stderr", result.stderr_text, output["stderr"], diags);
        }

        for (const auto &f : output.value("files", nlohmann::json::array()))
        {
            auto p = workdir / f.at("test-file").get<std::string>();

            auto exists = std::filesystem::exists(p);
            auto expected_exists = f.value("exists", false);

            if (exists != expected_exists)
            {
                diags.push_back({std::format("file check failed for '{}': "
                                             "expected exists={}, received exists={}",
                                             p.string(), expected_exists, exists)});
                ok = false;
            }
        }

        // Prepare JSON result
        auto test_result = nlohmann::json::object();
        test_result["name"] = test_name;
        test_result["passed"] = ok;

        if (!ok)
        {
            auto error_messages = nlohmann::json::array();
            for (const auto &d : diags)
            {
                error_messages.push_back(d.message);
            }
            test_result["errors"] = error_messages;
        }

        json_results.push_back(test_result);

        if (!options.json_output)
        {
            if (ok)
            {
                spdlog::info("PASS");
            }
            else
            {
                spdlog::error("FAIL");
                for (const auto &d : diags)
                {
                    spdlog::error("  {}", d.message);
                }
            }
        }
    }

    // Output JSON if requested
    if (options.json_output)
    {
        std::cout << json_results.dump(2) << std::endl;
    }
}

/// @brief Main entry point for the rsc-test tool.
/// @param argc Number of command-line arguments
/// @param argv Array of command-line argument strings
/// @return Exit code (0 for success, non-zero for failure)
auto main(const int argc, const char **argv) -> int
{
    auto options = cli(argc, argv);
    run(options);
}
