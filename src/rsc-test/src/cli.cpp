#include "rsc_test/cli.hpp"

#include <CLI/CLI.hpp>

#include <cstdlib>
#include <iostream>
#include <print>
#include <regex>

namespace rsc
{
auto cli(const int argc, const char **argv) -> Options
{
    auto app = CLI::App{"rsc-test"};
    auto options = Options{};
    app.add_option("test_configuration", options.test_configuration)->required();
    app.add_option("--filter", options.filter_pattern, "Filter test names using regex pattern");
    app.add_flag("--list", options.list_tests, "List test names without running them");
    app.add_flag("--generate", options.generate, "Generate a new test file");
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
