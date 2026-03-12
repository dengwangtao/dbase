#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_approx.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "dbase/error/error.h"
#include "dbase/str/str.h"

namespace
{
using dbase::ErrorCode;
namespace str = dbase::str;
}  // namespace

TEST_CASE("startsWith endsWith contains basic behavior", "[str]")
{
    REQUIRE(str::startsWith("abcdef", "abc"));
    REQUIRE_FALSE(str::startsWith("abcdef", "bcd"));

    REQUIRE(str::endsWith("abcdef", "def"));
    REQUIRE_FALSE(str::endsWith("abcdef", "cde"));

    REQUIRE(str::contains("abcdef", "bcd"));
    REQUIRE_FALSE(str::contains("abcdef", "xyz"));
}

TEST_CASE("equalsIgnoreCase compares ASCII case insensitively", "[str]")
{
    REQUIRE(str::equalsIgnoreCase("Hello", "hello"));
    REQUIRE(str::equalsIgnoreCase("DBASE", "dbase"));
    REQUIRE_FALSE(str::equalsIgnoreCase("abc", "abd"));
    REQUIRE_FALSE(str::equalsIgnoreCase("abc", "ab"));
}

TEST_CASE("toLower and toUpper transform ASCII letters", "[str]")
{
    REQUIRE(str::toLower("AbC123_XyZ") == "abc123_xyz");
    REQUIRE(str::toUpper("AbC123_XyZ") == "ABC123_XYZ");
}

TEST_CASE("trim view helpers return trimmed view", "[str]")
{
    REQUIRE(str::trimLeftView("  abc  ") == "abc  ");
    REQUIRE(str::trimRightView("  abc  ") == "  abc");
    REQUIRE(str::trimView("  abc  ") == "abc");
    REQUIRE(str::trimView("\r\n\t abc \t\r\n") == "abc");
}

TEST_CASE("trim string helpers return trimmed string copy", "[str]")
{
    REQUIRE(str::trimLeft("  abc  ") == "abc  ");
    REQUIRE(str::trimRight("  abc  ") == "  abc");
    REQUIRE(str::trim("  abc  ") == "abc");
    REQUIRE(str::trim("xxxxabcxxxx", "x") == "abc");
}

TEST_CASE("trim of all trim characters returns empty", "[str]")
{
    REQUIRE(str::trimView("   ").empty());
    REQUIRE(str::trim("   ").empty());
    REQUIRE(str::trimView("xxxx", "x").empty());
    REQUIRE(str::trim("xxxx", "x").empty());
}

TEST_CASE("removePrefixView and removeSuffixView remove only matching side", "[str]")
{
    REQUIRE(str::removePrefixView("prefix_body", "prefix_") == "body");
    REQUIRE(str::removePrefixView("body", "prefix_") == "body");

    REQUIRE(str::removeSuffixView("body_suffix", "_suffix") == "body");
    REQUIRE(str::removeSuffixView("body", "_suffix") == "body");
}

TEST_CASE("split by char without keeping empty parts", "[str]")
{
    const auto parts = str::split("a,,b,c,", ',', false);

    REQUIRE(parts.size() == 3);
    REQUIRE(parts[0] == "a");
    REQUIRE(parts[1] == "b");
    REQUIRE(parts[2] == "c");
}

TEST_CASE("split by char with keeping empty parts", "[str]")
{
    const auto parts = str::split("a,,b,c,", ',', true);

    REQUIRE(parts.size() == 5);
    REQUIRE(parts[0] == "a");
    REQUIRE(parts[1].empty());
    REQUIRE(parts[2] == "b");
    REQUIRE(parts[3] == "c");
    REQUIRE(parts[4].empty());
}

TEST_CASE("split by string delimiter works", "[str]")
{
    const auto parts = str::split("aa--bb----cc", "--", true);

    REQUIRE(parts.size() == 4);
    REQUIRE(parts[0] == "aa");
    REQUIRE(parts[1] == "bb");
    REQUIRE(parts[2].empty());
    REQUIRE(parts[3] == "cc");
}

TEST_CASE("splitView by char returns views into original string", "[str]")
{
    const std::string text = "one,two,,four";
    const auto parts = str::splitView(text, ',', true);

    REQUIRE(parts.size() == 4);
    REQUIRE(parts[0] == "one");
    REQUIRE(parts[1] == "two");
    REQUIRE(parts[2].empty());
    REQUIRE(parts[3] == "four");
}

TEST_CASE("splitView by string delimiter works", "[str]")
{
    const std::string text = "ab::<>::cd::<>::";
    const auto parts = str::splitView(text, "::<>::", true);

    REQUIRE(parts.size() == 3);
    REQUIRE(parts[0] == "ab");
    REQUIRE(parts[1] == "cd");
    REQUIRE(parts[2].empty());
}

TEST_CASE("join string span joins parts with delimiter", "[str]")
{
    const std::vector<std::string> parts{"aa", "bb", "cc"};

    REQUIRE(str::join(parts, ",") == "aa,bb,cc");
    REQUIRE(str::join(parts, "") == "aabbcc");
}

TEST_CASE("join string_view span joins parts with delimiter", "[str]")
{
    const std::array<std::string_view, 3> parts{"x", "y", "z"};

    REQUIRE(str::join(std::span<const std::string_view>(parts), "-") == "x-y-z");
}

TEST_CASE("replaceFirst replaces first matching occurrence only", "[str]")
{
    REQUIRE(str::replaceFirst("foo bar foo", "foo", "baz") == "baz bar foo");
    REQUIRE(str::replaceFirst("hello", "xyz", "q") == "hello");
}

TEST_CASE("replaceAll replaces all matching occurrences", "[str]")
{
    REQUIRE(str::replaceAll("foo bar foo foo", "foo", "baz") == "baz bar baz baz");
    REQUIRE(str::replaceAll("hello", "xyz", "q") == "hello");
}

TEST_CASE("toInt parses valid int32 values", "[str]")
{
    const auto value = str::toInt(" 123 ");

    REQUIRE(value.hasValue());
    REQUIRE(value.value() == 123);
}

TEST_CASE("toInt rejects invalid int32 text", "[str]")
{
    const auto value = str::toInt("12x");

    REQUIRE(value.hasError());
    REQUIRE(value.error().code() == ErrorCode::ParseError);
    REQUIRE_THAT(value.error().message(), Catch::Matchers::ContainsSubstring("int32"));
}

TEST_CASE("toInt64 parses valid int64 values", "[str]")
{
    const auto value = str::toInt64(" -456 ");

    REQUIRE(value.hasValue());
    REQUIRE(value.value() == static_cast<std::int64_t>(-456));
}

TEST_CASE("toInt64 rejects invalid int64 text", "[str]")
{
    const auto value = str::toInt64("");

    REQUIRE(value.hasError());
    REQUIRE(value.error().code() == ErrorCode::ParseError);
    REQUIRE_THAT(value.error().message(), Catch::Matchers::ContainsSubstring("int64"));
}

TEST_CASE("toDouble parses valid floating point values", "[str]")
{
    const auto value = str::toDouble(" 3.25 ");

    REQUIRE(value.hasValue());
    REQUIRE(value.value() == Catch::Approx(3.25));
}

TEST_CASE("toDouble rejects invalid floating point text", "[str]")
{
    const auto value = str::toDouble("3.25x");

    REQUIRE(value.hasError());
    REQUIRE(value.error().code() == ErrorCode::ParseError);
    REQUIRE_THAT(value.error().message(), Catch::Matchers::ContainsSubstring("double"));
}

TEST_CASE("toBool parses supported true tokens", "[str]")
{
    REQUIRE(str::toBool("true").value());
    REQUIRE(str::toBool("1").value());
    REQUIRE(str::toBool(" yes ").value());
    REQUIRE(str::toBool("ON").value());
}

TEST_CASE("toBool parses supported false tokens", "[str]")
{
    REQUIRE_FALSE(str::toBool("false").value());
    REQUIRE_FALSE(str::toBool("0").value());
    REQUIRE_FALSE(str::toBool(" no ").value());
    REQUIRE_FALSE(str::toBool("Off").value());
}

TEST_CASE("toBool rejects unsupported text", "[str]")
{
    const auto value = str::toBool("maybe");

    REQUIRE(value.hasError());
    REQUIRE(value.error().code() == ErrorCode::ParseError);
    REQUIRE_THAT(value.error().message(), Catch::Matchers::ContainsSubstring("bool"));
}