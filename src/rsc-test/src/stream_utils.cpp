#include "rsc_test/stream_utils.hpp"

#include <algorithm>
#include <format>
#include <ranges>

namespace rsc
{
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
                  const bool empty, std::vector<rsc::Diagnostic> &diags) -> bool
{
    if (!exact)
    {
        actual = rsc::normalize_output(actual);
        expected = rsc::normalize_output(expected);
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
} // namespace rsc
