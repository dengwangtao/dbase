#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_approx.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "dbase/config/ini_config.h"
#include "dbase/error/error.h"

namespace
{
using dbase::ErrorCode;
using dbase::config::IniConfig;

std::filesystem::path makeTempIniPath(const std::string& name)
{
    return std::filesystem::temp_directory_path() / name;
}
}  // namespace

TEST_CASE("IniConfig parses flat key value pairs", "[config][ini_config]")
{
    const auto result = IniConfig::fromString(R"ini(
name = dbase
port = 9527
ratio = 3.5
enabled = true
)ini");

    REQUIRE(result.hasValue());

    const auto& config = result.value();
    REQUIRE(config.has("name"));
    REQUIRE(config.has("port"));
    REQUIRE(config.has("ratio"));
    REQUIRE(config.has("enabled"));

    REQUIRE(config.getString("name").value() == "dbase");
    REQUIRE(config.getInt("port").value() == 9527);
    REQUIRE(config.getDouble("ratio").value() == Catch::Approx(3.5));
    REQUIRE(config.getBool("enabled").value());
}

TEST_CASE("IniConfig parses sectioned keys as dotted names", "[config][ini_config]")
{
    const auto result = IniConfig::fromString(R"ini(
[server]
host = 127.0.0.1
port = 8080

[feature]
enabled = yes
)ini");

    REQUIRE(result.hasValue());

    const auto& config = result.value();
    REQUIRE(config.has("server.host"));
    REQUIRE(config.has("server.port"));
    REQUIRE(config.has("feature.enabled"));

    REQUIRE(config.getString("server.host").value() == "127.0.0.1");
    REQUIRE(config.getInt("server.port").value() == 8080);
    REQUIRE(config.getBool("feature.enabled").value());
}

TEST_CASE("IniConfig ignores blank lines and comment lines", "[config][ini_config]")
{
    const auto result = IniConfig::fromString(R"ini(

# comment
; another comment

[app]

name = demo

)ini");

    REQUIRE(result.hasValue());
    REQUIRE(result.value().getString("app.name").value() == "demo");
}

TEST_CASE("IniConfig trims surrounding whitespace around keys and values", "[config][ini_config]")
{
    const auto result = IniConfig::fromString(R"ini(
 [ section ] 
 key   =   value with spaces   
 number =   42
 flag =   off
)ini");

    REQUIRE(result.hasValue());

    const auto& config = result.value();
    REQUIRE(config.getString("section.key").value() == "value with spaces");
    REQUIRE(config.getInt("section.number").value() == 42);
    REQUIRE_FALSE(config.getBool("section.flag").value());
}

TEST_CASE("IniConfig parses boolean variants", "[config][ini_config]")
{
    const auto result = IniConfig::fromString(R"ini(
a = true
b = false
c = yes
d = no
e = on
f = off
g = 1
h = 0
)ini");

    REQUIRE(result.hasValue());

    const auto& config = result.value();
    REQUIRE(config.getBool("a").value());
    REQUIRE_FALSE(config.getBool("b").value());
    REQUIRE(config.getBool("c").value());
    REQUIRE_FALSE(config.getBool("d").value());
    REQUIRE(config.getBool("e").value());
    REQUIRE_FALSE(config.getBool("f").value());
    REQUIRE(config.getBool("g").value());
    REQUIRE_FALSE(config.getBool("h").value());
}

TEST_CASE("IniConfig parses integers and doubles", "[config][ini_config]")
{
    const auto result = IniConfig::fromString(R"ini(
int_value = -123
double_value = 6.25
)ini");

    REQUIRE(result.hasValue());

    const auto& config = result.value();
    REQUIRE(config.getInt("int_value").value() == -123);
    REQUIRE(config.getDouble("double_value").value() == Catch::Approx(6.25));
}

TEST_CASE("IniConfig stores non parseable scalar as string", "[config][ini_config]")
{
    const auto result = IniConfig::fromString(R"ini(
token = abc-123
)ini");

    REQUIRE(result.hasValue());

    const auto& config = result.value();
    REQUIRE(config.getString("token").value() == "abc-123");

    const auto intRet = config.getInt("token");
    REQUIRE(intRet.hasError());
    REQUIRE(intRet.error().code() == ErrorCode::InvalidArgument);
}

TEST_CASE("IniConfig later assignments overwrite earlier values", "[config][ini_config]")
{
    const auto result = IniConfig::fromString(R"ini(
name = first
name = second

[server]
port = 1
port = 2
)ini");

    REQUIRE(result.hasValue());

    const auto& config = result.value();
    REQUIRE(config.getString("name").value() == "second");
    REQUIRE(config.getInt("server.port").value() == 2);
}

TEST_CASE("IniConfig getOr helpers return fallback for missing keys", "[config][ini_config]")
{
    const auto result = IniConfig::fromString("name = demo");
    REQUIRE(result.hasValue());

    const auto& config = result.value();
    REQUIRE(config.getStringOr("name", "x") == "demo");
    REQUIRE(config.getStringOr("missing", "fallback") == "fallback");
    REQUIRE(config.getIntOr("missing", 7) == 7);
    REQUIRE(config.getDoubleOr("missing", 1.5) == Catch::Approx(1.5));
    REQUIRE(config.getBoolOr("missing", true));
}

TEST_CASE("IniConfig require succeeds for existing key and fails for missing key", "[config][ini_config]")
{
    const auto result = IniConfig::fromString("name = demo");
    REQUIRE(result.hasValue());

    const auto& config = result.value();
    REQUIRE(config.require("name").hasValue());

    const auto missing = config.require("missing");
    REQUIRE(missing.hasError());
    REQUIRE(missing.error().code() == ErrorCode::NotFound);
    REQUIRE_THAT(missing.error().message(), Catch::Matchers::ContainsSubstring("required config key not found"));
}

TEST_CASE("IniConfig values exposes parsed entries", "[config][ini_config]")
{
    const auto result = IniConfig::fromString(R"ini(
a = 1
[sec]
b = two
)ini");

    REQUIRE(result.hasValue());

    const auto& values = result.value().values();
    REQUIRE(values.size() == 2);
    REQUIRE(values.find("a") != values.end());
    REQUIRE(values.find("sec.b") != values.end());
}

TEST_CASE("IniConfig parse rejects invalid section syntax", "[config][ini_config]")
{
    const auto result = IniConfig::fromString(R"ini(
[broken
key = value
)ini");

    REQUIRE(result.hasError());
    REQUIRE(result.error().code() == ErrorCode::ParseError);
    REQUIRE_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("invalid section syntax"));
}

TEST_CASE("IniConfig parse rejects empty section name", "[config][ini_config]")
{
    const auto result = IniConfig::fromString(R"ini(
[   ]
key = value
)ini");

    REQUIRE(result.hasError());
    REQUIRE(result.error().code() == ErrorCode::ParseError);
    REQUIRE_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("empty section name"));
}

TEST_CASE("IniConfig parse rejects missing equals sign", "[config][ini_config]")
{
    const auto result = IniConfig::fromString(R"ini(
key value
)ini");

    REQUIRE(result.hasError());
    REQUIRE(result.error().code() == ErrorCode::ParseError);
    REQUIRE_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("missing '='"));
}

TEST_CASE("IniConfig parse rejects empty key", "[config][ini_config]")
{
    const auto result = IniConfig::fromString(R"ini(
= value
)ini");

    REQUIRE(result.hasError());
    REQUIRE(result.error().code() == ErrorCode::ParseError);
    REQUIRE_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("empty key"));
}

TEST_CASE("IniConfig fromFile reads valid file", "[config][ini_config]")
{
    const auto path = makeTempIniPath("dbase_ini_config_test_valid.ini");

    {
        std::ofstream ofs(path);
        ofs << "[server]\n";
        ofs << "host = 127.0.0.1\n";
        ofs << "port = 9000\n";
    }

    const auto result = IniConfig::fromFile(path);

    REQUIRE(result.hasValue());
    REQUIRE(result.value().getString("server.host").value() == "127.0.0.1");
    REQUIRE(result.value().getInt("server.port").value() == 9000);

    std::filesystem::remove(path);
}

TEST_CASE("IniConfig fromFile returns error for missing file", "[config][ini_config]")
{
    const auto path = makeTempIniPath("dbase_ini_config_test_missing.ini");
    std::filesystem::remove(path);

    const auto result = IniConfig::fromFile(path);

    REQUIRE(result.hasError());
    REQUIRE(result.error().code() != ErrorCode::Ok);
}

TEST_CASE("IniConfig preserves line number in parse errors", "[config][ini_config]")
{
    const auto result = IniConfig::fromString(R"ini(
first = ok
broken line
)ini");

    REQUIRE(result.hasError());
    REQUIRE(result.error().code() == ErrorCode::ParseError);
    REQUIRE_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("line 3"));
}