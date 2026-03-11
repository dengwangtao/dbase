#include "dbase/log/log.h"
#include "dbase/random/random.h"

#include <array>
#include <string>
#include <vector>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    dbase::random::Random rng(123456);

    DBASE_LOG_INFO("nextU64={}", rng.nextU64());
    DBASE_LOG_INFO("nextU32={}", rng.nextU32());
    DBASE_LOG_INFO("uniformInt[1,10]={}", rng.uniformInt(1, 10));
    DBASE_LOG_INFO("uniformInt64[100,1000]={}", rng.uniformInt64(100, 1000));
    DBASE_LOG_INFO("uniformReal[0,1]={}", rng.uniformReal());
    DBASE_LOG_INFO("bernoulli(0.25)={}", rng.bernoulli(0.25));

    std::vector<int> values = {1, 2, 3, 4, 5, 6};
    rng.shuffle(values.begin(), values.end());

    std::string shuffled;
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i != 0)
        {
            shuffled += ",";
        }
        shuffled += std::to_string(values[i]);
    }
    DBASE_LOG_INFO("shuffled={}", shuffled);

    const std::array<const char*, 4> names = {"alpha", "beta", "gamma", "delta"};
    DBASE_LOG_INFO("choice={}", rng.choice<const char*>(names));

    const std::array<int, 4> weights = {10, 20, 30, 40};
    DBASE_LOG_INFO("weightedIndex={}", rng.weightedIndex<int>(weights));

    DBASE_LOG_INFO("threadLocal uniformInt={}", dbase::random::uniformInt(1, 100));
    DBASE_LOG_INFO("threadLocal uniformReal={}", dbase::random::uniformReal(10.0, 20.0));

    return 0;
}