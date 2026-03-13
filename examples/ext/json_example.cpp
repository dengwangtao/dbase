#include "ext/toml++/toml.h"
#include "ext/nlohmann/json.hpp"
#include "dbase/log/log.h"

int main()
{
    nlohmann::json j = R"json({
"name": "wangtao.deng",
"age": 25,
"hobbies": [
    "reading",
    "swimming",
    "traveling",
    "coding"
    ]
})json"_json;


    auto s = j.dump();

    DBASE_LOG_INFO("json: {}", s);

    return 0;
}