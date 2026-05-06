#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <regex>
#include <set>
#include <string>

namespace rsc
{
auto read_configuration(const std::filesystem::path &test_configuration) -> nlohmann::json;
auto generate_configuration(const std::filesystem::path &test_configuration) -> void;
auto get_tests(const std::regex &filter_pattern, const nlohmann::json::array_t &tests) -> std::set<std::string>;
} // namespace rsc
