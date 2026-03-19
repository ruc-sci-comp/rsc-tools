#include "rsc_test/stream_utils.hpp"

#include <algorithm>
#include <format>

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

    if (!expected.empty())
    {
        if (actual != expected)
        {
            diags.push_back({std::format("{} mismatch:\n"
                                         "    expected: \"{}\"\n"
                                         "    received: \"{}\"",
                                         label, expected, actual)});
            return false;
        }
    }

    return true;
}
} // namespace rsc
