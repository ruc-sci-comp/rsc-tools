#include "rsc_test/cli.hpp"
#include "rsc_test/runner.hpp"

auto main(const int argc, const char **argv) -> int
{
    auto options = rsc::cli(argc, argv);
    return rsc::run(options) ? 0 : 1;
}
