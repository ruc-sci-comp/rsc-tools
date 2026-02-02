#include <CLI/CLI.hpp>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstdlib>
#include <string>

struct Options
{
    std::string token{};
    std::string name{};
    std::string description{};
    std::string org{};
    bool is_private{true};
    bool is_template{true};
};

auto cli(const int argc, const char **argv) -> Options
{
    auto app = CLI::App{"rsc-repo"};
    auto options = Options{};
    app.add_option("--token", options.token, "GitHub API token")->required()->envname("GITHUB_TOKEN");
    app.add_option("--name", options.name, "Repository name")->required();
    app.add_option("--description", options.description, "Repository description");
    app.add_option("--org", options.org, "GitHub organization (omit to create under user)");
    app.add_flag("--private,!--no-private", options.is_private, "Create a private repository");
    app.add_flag("--template,!--no-template", options.is_template, "Mark repository as a template");

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
    auto payload = nlohmann::json{{"name", options.name},
                                  {"description", options.description},
                                  {"private", options.is_private},
                                  {"is_template", options.is_template},
                                  {"auto_init", true}};

    auto url = options.org.empty() ? cpr::Url{"https://api.github.com/user/repos"}
                                   : cpr::Url{"https://api.github.com/orgs/" + options.org + "/repos"};

    auto response = cpr::Post(url,
                              cpr::Header{{"Authorization", "Bearer " + options.token},
                                          {"Accept", "application/vnd.github+json"},
                                          {"Content-Type", "application/json"},
                                          {"User-Agent", "rsc-init"}},
                              cpr::Body{payload.dump()});

    if (response.error)
    {
        spdlog::error("HTTP error: {}", response.error.message);
        std::exit(1);
    }

    if (response.status_code != 201)
    {
        spdlog::error("GitHub API error ({})", response.status_code);
        spdlog::error("{}", response.text);
        std::exit(1);
    }

    auto json = nlohmann::json::parse(response.text);
    spdlog::info("Repository created: {}", json.at("html_url").get<std::string>());
}

auto main(const int argc, const char **argv) -> int
{
    auto options = cli(argc, argv);
    run(options);
}