#pragma once

#include <filesystem>
#include <regex>

namespace rsc
{
struct Options
{
    std::filesystem::path test_configuration{};
    std::regex filter_pattern{".*"};
    bool list_tests{};
    bool generate{};
};

auto cli(int argc, const char **argv) -> Options;
} // namespace rsc
