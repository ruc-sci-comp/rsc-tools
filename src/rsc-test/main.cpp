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

struct Options
{
    std::filesystem::path test_configuration{};
    std::string filter_pattern{};
    bool list_tests{};
    bool json_output{};
};

struct Diagnostic
{
    std::string message{};
};

struct RunResult
{
    std::string stdout_text{};
    std::string stderr_text{};
    int returncode{};
};

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
    if (!stdin_data.empty() && stdin_data.back() != '\n') {
        stdin_data += '\n';
    }

    // Check if executable exists and is accessible
    if (!std::filesystem::exists(executable))
    {
        spdlog::error("ERROR: Executable '{}' does not exist", executable.string());
        spdlog::error("       Tried to run from working directory: {}", workdir.string());
        throw std::runtime_error("Executable not found: " + executable.string());
    }

    if (!std::filesystem::is_regular_file(executable))
    {
        spdlog::error("ERROR: '{}' is not a regular file", executable.string());
        throw std::runtime_error("Executable is not a regular file: " + executable.string());
    }

    if ((std::filesystem::status(executable).permissions() & std::filesystem::perms::owner_exec) ==
        std::filesystem::perms::none)
    {
        spdlog::error("ERROR: '{}' does not have execute permissions", executable.string());
        throw std::runtime_error("Executable does not have execute permissions: " + executable.string());
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
            executable = std::filesystem::canonical(options.test_configuration.parent_path() / executable);
            // Create symlink with the original executable name in the working directory
            auto symlink_path = workdir / executable.filename();
            try
            {
                std::filesystem::create_symlink(executable, symlink_path);
            }
            catch (const std::filesystem::filesystem_error &e)
            {
                spdlog::warn("Failed to create symlink for '{}': {}", executable.string(), e.what());
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

auto main(const int argc, const char **argv) -> int
{
    auto options = cli(argc, argv);
    run(options);
}
