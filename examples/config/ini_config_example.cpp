#include "dbase/config/ini_config.h"
#include "dbase/log/log.h"

#include <string_view>
#include <utility>

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
tags.0 = gateway
tags.1 = internal
ports.0 = 8080
ports.1 = 8081

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
        DBASE_LOG_ERROR("load ini config failed: {}", configRet.error().toString());
        return 1;
    }

    auto config = std::move(configRet.value());

    auto host = config.getString("server.host");
    auto port = config.getInt("server.port");
    auto workers = config.getInt("server.workers");
    auto ssl = config.getBool("server.enable_ssl");
    auto tag0 = config.getString("server.tags.0");
    auto tag1 = config.getString("server.tags.1");
    auto port0 = config.getUInt("server.ports.0");
    auto port1 = config.getUInt("server.ports.1");
    auto tags = config.getArray("server.tags");
    auto ports = config.getArray("server.ports");
    auto serverObj = config.getObject("server");
    auto logLevel = config.getString("log.level");
    auto flushInterval = config.getInt("log.flush_interval");
    auto appName = config.getString("app.name");
    auto pi = config.getDouble("app.pi");

    if (!host || !port || !workers || !ssl || !tag0 || !tag1 || !port0 || !port1 || !tags || !ports || !serverObj || !logLevel || !flushInterval || !appName || !pi)
    {
        DBASE_LOG_ERROR("read ini config failed");
        return 1;
    }

    DBASE_LOG_INFO("server.host={}", host.value());
    DBASE_LOG_INFO("server.port={}", port.value());
    DBASE_LOG_INFO("server.workers={}", workers.value());
    DBASE_LOG_INFO("server.enable_ssl={}", ssl.value());
    DBASE_LOG_INFO("server.tags.0={}", tag0.value());
    DBASE_LOG_INFO("server.tags.1={}", tag1.value());
    DBASE_LOG_INFO("server.ports.0={}", port0.value());
    DBASE_LOG_INFO("server.ports.1={}", port1.value());
    DBASE_LOG_INFO("server.tags size={}", tags.value()->size());
    DBASE_LOG_INFO("server.ports size={}", ports.value()->size());
    DBASE_LOG_INFO("server object field count={}", serverObj.value()->size());
    DBASE_LOG_INFO("log.level={}", logLevel.value());
    DBASE_LOG_INFO("log.flush_interval={}", flushInterval.value());
    DBASE_LOG_INFO("app.name={}", appName.value());
    DBASE_LOG_INFO("app.pi={}", pi.value());

    DBASE_LOG_INFO("missing.string default={}", config.getStringOr("missing.string", "fallback"));
    DBASE_LOG_INFO("missing.int default={}", config.getIntOr("missing.int", 42));
    DBASE_LOG_INFO("missing.uint default={}", config.getUIntOr("missing.uint", 7));
    DBASE_LOG_INFO("missing.double default={}", config.getDoubleOr("missing.double", 6.28));
    DBASE_LOG_INFO("missing.bool default={}", config.getBoolOr("missing.bool", true));

    auto requireHostRet = config.require("server.host");
    if (!requireHostRet)
    {
        DBASE_LOG_ERROR("require server.host failed: {}", requireHostRet.error().toString());
        return 1;
    }

    auto requireTagsRet = config.require("server.tags.1");
    if (!requireTagsRet)
    {
        DBASE_LOG_ERROR("require server.tags.1 failed: {}", requireTagsRet.error().toString());
        return 1;
    }

    DBASE_LOG_INFO("ini config item count={}", config.values().size());
    for (const auto& [key, value] : config.values())
    {
        DBASE_LOG_INFO("item: {} = {}", key, value.toString());
    }

    return 0;
}