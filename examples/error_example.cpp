#include "dbase/error/error.h"
#include "dbase/log/log.h"

#include <string>

namespace
{
dbase::Result<int> parsePositiveInt(const std::string& text)
{
    if (text.empty())
    {
        return dbase::makeErrorResult<int>(dbase::ErrorCode::InvalidArgument, "input is empty");
    }

    int value = 0;
    for (char ch : text)
    {
        if (ch < '0' || ch > '9')
        {
            return dbase::makeErrorResult<int>(dbase::ErrorCode::ParseError, "input is not numeric");
        }

        value = value * 10 + (ch - '0');
    }

    if (value <= 0)
    {
        return dbase::makeErrorResult<int>(dbase::ErrorCode::InvalidArgument, "value must be positive");
    }

    return value;
}

dbase::Result<void> doWork(bool fail)
{
    if (fail)
    {
        return dbase::makeError(dbase::ErrorCode::InvalidState, "worker is not ready");
    }

    return {};
}
}  // namespace

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    auto r1 = parsePositiveInt("123");
    if (r1)
    {
        DBASE_LOG_INFO("parse result={}", r1.value());
    }

    auto r2 = parsePositiveInt("12x");
    if (!r2)
    {
        DBASE_LOG_ERROR("parse failed: {}", r2.error().toString());
    }

    auto r3 = doWork(false);
    if (r3)
    {
        DBASE_LOG_INFO("doWork success");
    }

    auto r4 = doWork(true);
    if (!r4)
    {
        DBASE_LOG_ERROR("doWork failed: {}", r4.error().toString());
    }

    auto fallback = parsePositiveInt("bad").valueOr(42);
    DBASE_LOG_INFO("fallback value={}", fallback);

    return 0;
}