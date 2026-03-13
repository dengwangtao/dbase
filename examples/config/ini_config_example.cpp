#include "dbase/config/ini_config.h"
#include "dbase/log/log.h"

#include <string_view>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    constexpr std::string_view iniText = R"ini(
[server]
host = 127.0.0.1
port = 9781
workers = 4
enable_ssl = yes

[log]
level = info
flush_interval = 3

[app]
name = dbase-demo
pi = 3.14159
)ini";

    auto configRet = dbase::config::IniConfig::fromString(iniText);
    if (!configRet)
    {
        DBASE_LOG_ERROR("load config failed: {}", configRet.error().toString());
        return 1;
    }

    auto config = std::move(configRet.value());

    auto host = config.getString("server.host");
    auto port = config.getInt("server.port");
    auto workers = config.getInt("server.workers");
    auto ssl = config.getBool("server.enable_ssl");
    auto pi = config.getDouble("app.pi");

    if (!host || !port || !workers || !ssl || !pi)
    {
        DBASE_LOG_ERROR("config read failed");
        return 1;
    }

    DBASE_LOG_INFO("server.host={}", host.value());
    DBASE_LOG_INFO("server.port={}", port.value());
    DBASE_LOG_INFO("server.workers={}", workers.value());
    DBASE_LOG_INFO("server.enable_ssl={}", ssl.value());
    DBASE_LOG_INFO("app.pi={}", pi.value());

    DBASE_LOG_INFO("log.level={}", config.getStringOr("log.level", "debug"));
    DBASE_LOG_INFO("missing.int default={}", config.getIntOr("missing.int", 42));
    DBASE_LOG_INFO("missing.bool default={}", config.getBoolOr("missing.bool", true));

    auto requireRet = config.require("server.host");
    if (!requireRet)
    {
        DBASE_LOG_ERROR("require failed: {}", requireRet.error().toString());
        return 1;
    }

    DBASE_LOG_INFO("config item count={}", config.values().size());
    return 0;
}