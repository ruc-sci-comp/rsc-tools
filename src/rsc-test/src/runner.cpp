#include "rsc_test/runner.hpp"
#include "rsc_test/config.hpp"

extern "C"
{
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <cpp-subprocess/subprocess.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <ranges>
#include <regex>
#include <set>
#include <string>
#include <vector>

using namespace std::literals;

namespace rsc
{

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

auto check_stream(const std::string &label, std::string actual, std::string expected, const bool exact,
                  const bool empty, std::vector<Diagnostic> &diags) -> bool
{
    if (!exact)
    {
        actual = normalize_output(actual);
        expected = normalize_output(expected);
    }

    if (empty)
    {
        if (!actual.empty())
        {
            diags.push_back({std::format("{} expected empty, received {} bytes", label, actual.size())});
            return false;
        }
        return true;
    }

    if (actual != expected)
    {
        auto split_lines = [](const std::string &s) {
            return s | std::views::split('\n')
                     | std::views::transform([](auto rng) { return std::string(rng.begin(), rng.end()); })
                     | std::ranges::to<std::vector<std::string>>();
        };

        auto exp_lines = split_lines(expected);
        auto act_lines = split_lines(actual);
        auto n = std::max(exp_lines.size(), act_lines.size());
        for (auto i = size_t{0}; i < n; ++i)
        {
            const auto &exp = i < exp_lines.size() ? exp_lines[i] : std::string{};
            const auto &act = i < act_lines.size() ? act_lines[i] : std::string{};
            if (exp != act)
            {
                diags.push_back({std::format("{} mismatch at line {}:\n"
                                             "    expected: \"{}\"\n"
                                             "    received: \"{}\"",
                                             label, i + 1, exp, act)});
                break;
            }
        }
        return false;
    }

    return true;
}

auto run_validation_script(const std::filesystem::path &script, const RunResult &result,
                           const std::filesystem::path &workdir) -> std::optional<Diagnostic>
{
    auto L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dofile(L, script.c_str()) != LUA_OK)
    {
        auto err = std::string(lua_tostring(L, -1));
        lua_close(L);
        throw std::runtime_error(std::format("Lua script error: {}", err));
    }

    lua_getglobal(L, "validate");

    if (!lua_isfunction(L, -1))
    {
        lua_close(L);
        throw std::runtime_error("Lua script must define function validate(stdout, stderr, workdir)");
    }

    lua_pushlstring(L, result.stdout_text.data(), result.stdout_text.size());
    lua_pushlstring(L, result.stderr_text.data(), result.stderr_text.size());
    lua_pushstring(L, workdir.c_str());

    if (lua_pcall(L, 3, 2, 0) != LUA_OK)
    {
        auto err = std::string(lua_tostring(L, -1));
        lua_close(L);
        throw std::runtime_error(std::format("Lua runtime error: {}", err));
    }

    if (!lua_isboolean(L, -2))
    {
        lua_close(L);
        throw std::runtime_error("validate() must at least return a boolean");
    }

    auto passed = lua_toboolean(L, -2);
    if (!passed)
    {
        if (lua_gettop(L) != 2 || !lua_isstring(L, -1))
        {
            lua_close(L);
            return Diagnostic{"Unknown validation error, failure reason not provided"};
        }
        auto diag = Diagnostic{std::format("Script validation failed:{}", lua_tostring(L, -1))};
        lua_close(L);
        return diag;
    }
    lua_close(L);
    return std::nullopt;
}

auto execute_test(const nlohmann::json &test, const std::filesystem::path &workdir) -> RunResult
{
    auto executable = test.at("executable").get<std::filesystem::path>();

    auto cmd = std::vector<std::string>{executable.string()};

    auto input_cfg = test.value("input", nlohmann::json::object());
    cmd.append_range(input_cfg.value("argv", std::vector<std::string>{}));

    auto env_map = input_cfg.value("env", subprocess::env_map_t{});

    auto stdin_data = input_cfg.value("stdin", std::vector<std::string>{}) | std::views::join_with(std::string("\n")) |
                      std::ranges::to<std::string>();

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

auto run(const Options &options) -> bool
{
    if (options.generate)
    {
        generate_configuration(options.test_configuration);
        return true;
    }

    auto config = [&options] {
        try
        {
            return read_configuration(options.test_configuration);
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
            return get_tests(options.filter_pattern, config.at("tests"));
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
        return true;
    }

    auto all_passed = true;

    for (const auto &test : config["tests"])
    {
        auto test_name = test.at("name").get<std::string>();
        if (!tests.contains(test_name))
        {
            continue;
        }

        spdlog::info("Running test: {}", test_name);

        auto workdir = std::filesystem::temp_directory_path() / "rsc-test";

        std::filesystem::remove_all(workdir);
        std::filesystem::create_directories(workdir);

        auto executable = test.at("executable").get<std::filesystem::path>();
        if (!std::filesystem::exists(executable))
        {
            auto msg = std::format("executable '{}' does not exist", executable.string());
            all_passed = false;
            spdlog::error("FAIL");
            spdlog::error("  {}", msg);
            continue;
        }

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

        auto result = execute_test(test, workdir);

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
            ok &= check_stream("stdout", result.stdout_text, expected, channel_configuration.value("exact", false),
                               channel_configuration.value("empty", false), diags);
        }

        if (output.contains("stderr"))
        {
            const auto &channel_configuration = output.at("stderr");
            auto expected = parse_channel(channel_configuration);
            ok &= check_stream("stderr", result.stderr_text, expected, channel_configuration.value("exact", false),
                               channel_configuration.value("empty", false), diags);
        }

        for (const auto &f : output.value("files", nlohmann::json::array()))
        {
            auto filename = f.at("test_file").get<std::string>();
            auto p = workdir / filename;
            auto label = std::format("file '{}'", filename);

            if (!std::filesystem::exists(p))
            {
                diags.push_back({std::format("{} does not exist", label)});
                ok = false;
                continue;
            }

            auto fh = std::ifstream(p, std::ios::binary | std::ios::ate);
            auto size = fh.tellg();
            auto actual = std::string(size, '\0');
            fh.seekg(0);
            fh.read(actual.data(), size);

            auto expected = parse_channel(f);
            ok &= check_stream(label, actual, expected, f.value("exact", false), f.value("empty", false), diags);
        }

        if (test.contains("script"))
        {
            auto script = test["script"].at("file").get<std::filesystem::path>();
            auto script_result = run_validation_script(script, result, workdir);
            if (script_result)
            {
                ok = false;
                diags.push_back(script_result.value());
            }
        }

        if (!ok)
        {
            all_passed = false;
        }

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

    return all_passed;
}
} // namespace rsc
