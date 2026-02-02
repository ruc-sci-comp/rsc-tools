#include <CLI/CLI.hpp>

#include <filesystem>
#include <fstream>
#include <print>
#include <regex>
#include <string>

struct Options
{
    std::filesystem::path directory{"."};
    std::string name{"foo"};
};

auto cli(const int argc, const char **argv) -> Options
{
    auto app = CLI::App{"rsc-init"};
    auto options = Options{};
    app.add_option("directory", options.directory, "Where to initialize a new project");
    app.add_option("name", options.name, "The name of the project");
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

auto run(const Options &options) -> void
{
    auto target_directory = options.directory / options.name;

    std::filesystem::create_directories(options.directory);
    std::filesystem::copy("/rsc/templates/project", target_directory, std::filesystem::copy_options::recursive);

    auto pattern = std::regex{"\\{\\{name\\}\\}"};
    for (const auto &entry : std::filesystem::recursive_directory_iterator(target_directory))
    {
        if (std::filesystem::is_regular_file(entry.path()))
        {
            auto ifile = std::ifstream(entry.path());
            auto content = std::string(std::istreambuf_iterator<char>{ifile}, std::istreambuf_iterator<char>{});
            ifile.close();

            auto result = std::regex_replace(content, pattern, options.name);
            auto ofile = std::ofstream(entry.path());
            ofile.write(result.data(), result.size());
        }
    }
}

auto main(const int argc, const char **argv) -> int
{
    auto options = cli(argc, argv);
    run(options);
}
