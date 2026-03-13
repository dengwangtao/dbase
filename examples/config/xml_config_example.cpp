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
        <listen_port>8080</listen_port>
        <listen_port>8081</listen_port>
        <limits>
            <max_conn>10000</max_conn>
            <timeout_ms>1500</timeout_ms>
        </limits>
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

    auto rootObj = config.getObject("config");
    auto serverObj = config.getObject("config.server");
    auto workerArray = config.getArray("config.server.worker");
    auto listenPortArray = config.getArray("config.server.listen_port");
    auto limitsObj = config.getObject("config.server.limits");

    auto host = config.getString("config.server.@host");
    auto port = config.getUInt("config.server.@port");
    auto workers = config.getUInt("config.server.workers");
    auto ssl = config.getBool("config.server.enable_ssl");
    auto worker0 = config.getString("config.server.worker.0");
    auto worker1 = config.getString("config.server.worker.1");
    auto listenPort0 = config.getUInt("config.server.listen_port.0");
    auto listenPort1 = config.getUInt("config.server.listen_port.1");
    auto maxConn = config.getUInt("config.server.limits.max_conn");
    auto timeoutMs = config.getUInt("config.server.limits.timeout_ms");
    auto logLevel = config.getString("config.log.level");
    auto flushInterval = config.getInt("config.log.flush_interval");
    auto appName = config.getString("config.app.name");
    auto pi = config.getDouble("config.app.pi");

    if (!rootObj || !serverObj || !workerArray || !listenPortArray || !limitsObj || !host || !port || !workers || !ssl || !worker0 || !worker1 || !listenPort0 || !listenPort1 || !maxConn || !timeoutMs || !logLevel || !flushInterval || !appName || !pi)
    {
        DBASE_LOG_ERROR("read xml config failed");
        return 1;
    }

    DBASE_LOG_INFO("config object field count={}", rootObj.value()->size());
    DBASE_LOG_INFO("config.server field count={}", serverObj.value()->size());
    DBASE_LOG_INFO("config.server.worker size={}", workerArray.value()->size());
    DBASE_LOG_INFO("config.server.listen_port size={}", listenPortArray.value()->size());
    DBASE_LOG_INFO("config.server.limits field count={}", limitsObj.value()->size());

    DBASE_LOG_INFO("config.server.@host={}", host.value());
    DBASE_LOG_INFO("config.server.@port={}", port.value());
    DBASE_LOG_INFO("config.server.workers={}", workers.value());
    DBASE_LOG_INFO("config.server.enable_ssl={}", ssl.value());
    DBASE_LOG_INFO("config.server.worker.0={}", worker0.value());
    DBASE_LOG_INFO("config.server.worker.1={}", worker1.value());
    DBASE_LOG_INFO("config.server.listen_port.0={}", listenPort0.value());
    DBASE_LOG_INFO("config.server.listen_port.1={}", listenPort1.value());
    DBASE_LOG_INFO("config.server.limits.max_conn={}", maxConn.value());
    DBASE_LOG_INFO("config.server.limits.timeout_ms={}", timeoutMs.value());
    DBASE_LOG_INFO("config.log.level={}", logLevel.value());
    DBASE_LOG_INFO("config.log.flush_interval={}", flushInterval.value());
    DBASE_LOG_INFO("config.app.name={}", appName.value());
    DBASE_LOG_INFO("config.app.pi={}", pi.value());

    DBASE_LOG_INFO("missing.string default={}", config.getStringOr("missing.string", "fallback"));
    DBASE_LOG_INFO("missing.int default={}", config.getIntOr("missing.int", 42));
    DBASE_LOG_INFO("missing.uint default={}", config.getUIntOr("missing.uint", 7));
    DBASE_LOG_INFO("missing.double default={}", config.getDoubleOr("missing.double", 6.28));
    DBASE_LOG_INFO("missing.bool default={}", config.getBoolOr("missing.bool", true));

    auto requireRet = config.require("config.server.@host");
    if (!requireRet)
    {
        DBASE_LOG_ERROR("require config.server.@host failed: {}", requireRet.error().toString());
        return 1;
    }

    auto requireWorkerRet = config.require("config.server.worker.1");
    if (!requireWorkerRet)
    {
        DBASE_LOG_ERROR("require config.server.worker.1 failed: {}", requireWorkerRet.error().toString());
        return 1;
    }

    DBASE_LOG_INFO("xml config item count={}", config.values().size());
    for (const auto& [key, value] : config.values())
    {
        DBASE_LOG_INFO("item: {} = {}", key, value.toString());
    }

    return 0;
}