#include "ext/toml++/toml.h"
#include "dbase/log/log.h"

int main()
{
    auto tbl = toml::table{
        { "lib", "toml++" },
        { "cpp", toml::array{ 17, 20, "and beyond" } },
        { "toml", toml::array{ "1.0.0", "and beyond" } },
        { "repo", "https://github.com/marzer/tomlplusplus/" },
        { "author", toml::table{
                { "name", "Mark Gillard" },
                { "github", "https://github.com/marzer" },
                { "twitter", "https://twitter.com/marzer8789" }
            }
        },
    };

    std::ostringstream oss;
    oss << tbl;

    DBASE_LOG_INFO("toml: \n{}", oss.str());

    oss.clear();

    oss << toml::json_formatter{ tbl };
    DBASE_LOG_INFO("to json: \n{}", oss.str());

    oss.clear();

    oss << toml::yaml_formatter{ tbl };
    DBASE_LOG_INFO("to yaml: \n{}", oss.str());
    oss.clear();

    return 0;
}