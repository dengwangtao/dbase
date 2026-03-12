#include <catch2/catch_test_macros.hpp>

#include <string>

#include "dbase/platform/process.h"

namespace
{
namespace pf = dbase::platform;
}

TEST_CASE("current_process pid is nonzero", "[process][current_process]")
{
    REQUIRE(pf::pid() != 0);
}

TEST_CASE("current_process parent pid is nonzero or nonnegative", "[process][current_process]")
{
    REQUIRE(pf::ppid() >= 0);
}

TEST_CASE("current_process executable path is not empty", "[process][current_process]")
{
    const auto exePath = pf::executablePath();

    REQUIRE(exePath.hasValue());
    REQUIRE_FALSE(exePath->empty());
}
