#include "dbase/config/xml_config.h"
#include "dbase/log/log.h"

#include <string_view>
#include <utility>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    constexpr std::string_view xmlText = R"xml(
<config>
    <server host="127.0.0.1" port="9781">
        <workers>4</workers>
        <enable_ssl>true</enable_ssl>
        <worker>io-0</worker>
        <worker>io-1</worker>
    </server>
    <log>
        <level>info</level>
        <flush_interval>3</flush_interval>
    </log>
    <app>
        <name>dbase-demo</name>
        <pi>3.14159</pi>
    </app>
</config>
)xml";

    auto configRet = dbase::config::XmlConfig::fromString(xmlText);
    if (!configRet)
    {
        DBASE_LOG_ERROR("load xml config failed: {}", configRet.error().toString());
        return 1;
    }

    auto config = std::move(configRet.value());

    auto root = config.getString("config");
    auto serverNode = config.getString("config.server");
    auto host = config.getString("config.server.@host");
    auto port = config.getInt("config.server.@port");
    auto workers = config.getInt("config.server.workers");
    auto ssl = config.getBool("config.server.enable_ssl");
    auto worker0 = config.getString("config.server.worker.0");
    auto worker1 = config.getString("config.server.worker.1");
    auto logLevel = config.getString("config.log.level");
    auto flushInterval = config.getInt("config.log.flush_interval");
    auto appName = config.getString("config.app.name");
    auto pi = config.getDouble("config.app.pi");

    if (!root || !serverNode || !host || !port || !workers || !ssl || !worker0 || !worker1 || !logLevel || !flushInterval || !appName || !pi)
    {
        DBASE_LOG_ERROR("read xml config failed");
        return 1;
    }

    DBASE_LOG_INFO("config={}", root.value());
    DBASE_LOG_INFO("config.server={}", serverNode.value());
    DBASE_LOG_INFO("config.server.@host={}", host.value());
    DBASE_LOG_INFO("config.server.@port={}", port.value());
    DBASE_LOG_INFO("config.server.workers={}", workers.value());
    DBASE_LOG_INFO("config.server.enable_ssl={}", ssl.value());
    DBASE_LOG_INFO("config.server.worker.0={}", worker0.value());
    DBASE_LOG_INFO("config.server.worker.1={}", worker1.value());
    DBASE_LOG_INFO("config.log.level={}", logLevel.value());
    DBASE_LOG_INFO("config.log.flush_interval={}", flushInterval.value());
    DBASE_LOG_INFO("config.app.name={}", appName.value());
    DBASE_LOG_INFO("config.app.pi={}", pi.value());

    DBASE_LOG_INFO("missing.string default={}", config.getStringOr("missing.string", "fallback"));
    DBASE_LOG_INFO("missing.int default={}", config.getIntOr("missing.int", 42));
    DBASE_LOG_INFO("missing.double default={}", config.getDoubleOr("missing.double", 6.28));
    DBASE_LOG_INFO("missing.bool default={}", config.getBoolOr("missing.bool", true));

    auto requireRet = config.require("config.server.@host");
    if (!requireRet)
    {
        DBASE_LOG_ERROR("require config.server.@host failed: {}", requireRet.error().toString());
        return 1;
    }

    DBASE_LOG_INFO("xml config item count={}", config.values().size());

    for (const auto& [key, value] : config.values())
    {
        DBASE_LOG_INFO("item: {} = {}", key, value.toString());
    }

    return 0;
}