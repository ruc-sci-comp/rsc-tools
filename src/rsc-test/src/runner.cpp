#include "rsc_test/runner.hpp"

#include "rsc_test/configuration.hpp"
#include "rsc_test/diagnostic.hpp"
#include "rsc_test/script.hpp"
#include "rsc_test/stream_utils.hpp"

#include <cpp-subprocess/subprocess.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <print>
#include <ranges>
#include <regex>
#include <string>
#include <vector>

using namespace std::literals;

namespace rsc
{
auto execute_test(const nlohmann::json &test, const std::filesystem::path &workdir) -> rsc::RunResult
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
        spdlog::debug("Executing: {} in directory: {}", executable.string(), workdir.string());

        auto p = subprocess::Popen{cmd,
                                   subprocess::cwd{workdir.string()},
                                   subprocess::environment{env_map},
                                   subprocess::output{subprocess::PIPE},
                                   subprocess::error{subprocess::PIPE},
                                   subprocess::input{subprocess::PIPE}};

        auto result = p.communicate(stdin_data);

        return rsc::RunResult{.stdout_text = std::string({result.first.buf.data(), result.first.length}),
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

auto run(const rsc::Options &options) -> void
{
    auto config = [&options] {
        try
        {
            return rsc::read_configuration(options.test_configuration);
        }
        catch (const std::exception &e)
        {
            spdlog::error("Validation of schema failed:\n{}", e.what());
            std::exit(1);
        }
    }();

    auto tests = [&options, &config] -> std::set<std::string> {
        try
        {
            return rsc::get_tests(options.filter_pattern, config.at("tests"));
        }
        catch (const std::regex_error &e)
        {
            spdlog::error("Invalid regex pattern:\n{}", e.what());
            std::exit(1);
        }
        catch (const std::runtime_error &e)
        {
            spdlog::error("Invalid test names:\n{}", e.what());
            std::exit(1);
        }
    }();

    if (options.list_tests)
    {
        spdlog::info("Available tests:\n{}", tests | std::views::join_with("\n"sv) | std::ranges::to<std::string>());
        return;
    }

    auto json_results = nlohmann::json::array();

    for (const auto &test : config["tests"])
    {
        auto test_name = test.at("name").get<std::string>();
        if (!tests.contains(test_name))
        {
            continue;
        }

        if (!options.json_output)
        {
            spdlog::info("Running test: {}", test_name);
        }

        auto workdir = std::filesystem::temp_directory_path() / "rsc-test";

        std::filesystem::remove_all(workdir);
        std::filesystem::create_directories(workdir);

        auto executable = test.at("executable").get<std::filesystem::path>();
        if (!executable.is_absolute())
        {
            auto abs_executable = std::filesystem::canonical(options.test_configuration.parent_path() / executable);
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

        auto result = rsc::execute_test(test, workdir);

        auto ok = true;
        auto diags = std::vector<rsc::Diagnostic>{};

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

        auto parse_channel = [](const nlohmann::json::object_t &channel_configuration) -> std::string {
            if (channel_configuration.contains("text"))
            {
                return channel_configuration.at("text").get<std::string>();
            }
            else if (channel_configuration.contains("from_file"))
            {
                auto path = channel_configuration.at("from_file").get<std::filesystem::path>();
                auto file = std::ifstream(path, std::ios::binary | std::ios::ate);
                auto size = file.tellg();
                auto buffer = std::string(size, '\0');
                file.seekg(0);
                file.read(buffer.data(), size);
                return buffer;
            }
            return "";
        };

        if (output.contains("stdout"))
        {
            const auto &channel_configuration = output.at("stdout");
            auto expected = parse_channel(channel_configuration);
            ok &= rsc::check_stream("stdout", result.stdout_text, expected, channel_configuration.value("exact", false),
                                    channel_configuration.value("empty", false), diags);
        }

        if (output.contains("stderr"))
        {
            const auto &channel_configuration = output.at("stderr");
            auto expected = parse_channel(channel_configuration);
            ok &= rsc::check_stream("stderr", result.stdout_text, expected, channel_configuration.value("exact", false),
                                    channel_configuration.value("empty", false), diags);
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

        if (test.contains("script"))
        {
            auto script = test["script"].at("file").get<std::filesystem::path>();
            auto script_result = rsc::run_validation_script(script, result, workdir);
            if (script_result)
            {
                ok = false;
                diags.push_back(script_result.value());
            }
        }

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

    if (options.json_output)
    {
        std::println("{}", json_results.dump(2));
    }
}
} // namespace rsc
