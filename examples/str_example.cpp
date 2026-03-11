#include "dbase/str/str.h"
#include "dbase/log/log.h"
#include "dbase/time/time.h"
#include "dbase/platform/process.h"

#include <iostream>

int main()
{
    const auto exe = dbase::platform::executablePath();
    if (exe)
    {
        DBASE_LOG_INFO("executable path={}", exe.value().string());
    }
    else
    {
        DBASE_LOG_ERROR("executable path failed: {}", exe.error().toString());
    }

    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    DBASE_LOG_INFO("startsWith={}", dbase::str::startsWith("hello world", "hello"));
    DBASE_LOG_INFO("endsWith={}", dbase::str::endsWith("hello world", "world"));
    DBASE_LOG_INFO("contains={}", dbase::str::contains("hello world", "lo wo"));
    DBASE_LOG_INFO("equalsIgnoreCase={}", dbase::str::equalsIgnoreCase("AbC", "aBc"));

    DBASE_LOG_INFO("toLower={}", dbase::str::toLower("HeLLo"));
    DBASE_LOG_INFO("toUpper={}", dbase::str::toUpper("HeLLo"));

    DBASE_LOG_INFO("trimLeft='{}'", dbase::str::trimLeft("   abc  "));
    DBASE_LOG_INFO("trimRight='{}'", dbase::str::trimRight("   abc  "));
    DBASE_LOG_INFO("trim='{}'", dbase::str::trim("   abc  "));

    DBASE_LOG_INFO("removePrefix='{}'", dbase::str::removePrefixView("prefix_value", "prefix_"));
    DBASE_LOG_INFO("removeSuffix='{}'", dbase::str::removeSuffixView("value.txt", ".txt"));

    const auto parts1 = dbase::str::split("a,b,,c", ',', true);
    DBASE_LOG_INFO("split(char) size={}", parts1.size());

    const auto parts2 = dbase::str::split("aa--bb----cc", "--", true);
    DBASE_LOG_INFO("split(string) size={}", parts2.size());

    const auto views = dbase::str::splitView("x|y||z", '|', true);
    std::vector<std::string_view> viewParts(views.begin(), views.end());
    DBASE_LOG_INFO("join(view)={}", dbase::str::join(std::span<const std::string_view>(viewParts), "/"));

    const std::vector<std::string> items = {"one", "two", "three"};
    DBASE_LOG_INFO("join(string)={}", dbase::str::join(std::span<const std::string>(items), ","));

    DBASE_LOG_INFO("replaceFirst={}", dbase::str::replaceFirst("a-b-c-b", "b", "X"));
    DBASE_LOG_INFO("replaceAll={}", dbase::str::replaceAll("a-b-c-b", "b", "X"));

    auto i32 = dbase::str::toInt("123");
    auto i64 = dbase::str::toInt64("456789");
    auto d = dbase::str::toDouble("3.14159");
    auto b = dbase::str::toBool("yes");

    if (i32)
    {
        DBASE_LOG_INFO("toInt={}", i32.value());
    }
    if (i64)
    {
        DBASE_LOG_INFO("toInt64={}", i64.value());
    }
    if (d)
    {
        DBASE_LOG_INFO("toDouble={}", d.value());
    }
    if (b)
    {
        DBASE_LOG_INFO("toBool={}", b.value());
    }

    auto bad = dbase::str::toBool("maybe");
    if (!bad)
    {
        DBASE_LOG_ERROR("toBool failed: {}", bad.error().toString());
    }

    return 0;
}