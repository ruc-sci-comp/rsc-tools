#include "rsc_test/configuration.hpp"

#include <CLI/CLI.hpp>
#include <nlohmann/json-schema.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <print>
#include <regex>
#include <stdexcept>

namespace rsc
{
constexpr char SCHEMA[] = {
#embed "schema.json"
};

auto read_configuration(const std::filesystem::path &test_configuration) -> nlohmann::json
{
    auto config = nlohmann::json::parse(std::ifstream{test_configuration});
    auto validator = nlohmann::json_schema::json_validator{};
    auto schema = nlohmann::json::parse(SCHEMA);
    validator.set_root_schema(schema);
    validator.validate(config);
    return config;
}

auto get_tests(const std::regex &filter_pattern, const nlohmann::json::array_t &tests) -> std::set<std::string>
{
    auto test_names = std::set<std::string>{};
    for (const auto &test : tests)
    {
        auto test_name = test.at("name").get<std::string>();
        if (!std::regex_search(test_name, filter_pattern))
        {
            continue;
        }
        if (test_names.contains(test_name))
        {
            throw std::runtime_error(std::format("Test names must be unique, found multiple \"{}\"", test_name));
        }
        test_names.insert(test_name);
    }
    return test_names;
}

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
    catch (const std::regex_error &e)
    {
        std::println(std::cerr, "Invalid regex pattern:\n{}", e.what());
        std::exit(1);
    }
    return options;
}
} // namespace rsc