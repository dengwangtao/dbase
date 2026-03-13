#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

#include "dbase/config/config.h"
#include "dbase/error/error.h"

namespace
{
using dbase::ErrorCode;
using dbase::config::Config;
using dbase::config::ConfigValue;

class TestConfig : public Config
{
    public:
        void set(std::string key, ConfigValue value)
        {
            Config::set(key, value);
        }
};

}  // namespace

TEST_CASE("Config default state is empty", "[config][config]")
{
    TestConfig config;

    REQUIRE(config.values().empty());
    REQUIRE_FALSE(config.has("name"));
}

TEST_CASE("Config set and has work for string value", "[config][config]")
{
    TestConfig config;

    config.set("name", ConfigValue(std::string("dbase")));

    REQUIRE(config.has("name"));
    REQUIRE_FALSE(config.has("missing"));
}

TEST_CASE("Config get returns stored ConfigValue", "[config][config]")
{
    TestConfig config;
    config.set("port", ConfigValue(std::int64_t(9527)));

    const auto value = config.get("port");

    REQUIRE(value.hasValue());
    REQUIRE(value.value()->isInt());
    REQUIRE(value.value()->asInt() == 9527);
}

TEST_CASE("Config get returns NotFound for missing key", "[config][config]")
{
    TestConfig config;

    const auto value = config.get("missing");

    REQUIRE(value.hasError());
    REQUIRE(value.error().code() == ErrorCode::NotFound);
}

TEST_CASE("Config getString returns stored string", "[config][config]")
{
    TestConfig config;
    config.set("name", ConfigValue(std::string("demo")));

    const auto value = config.getString("name");

    REQUIRE(value.hasValue());
    REQUIRE(value.value() == "demo");
}

TEST_CASE("Config getString returns type error on non string value", "[config][config]")
{
    TestConfig config;
    config.set("port", ConfigValue(std::int64_t(8080)));

    const auto value = config.getString("port");

    REQUIRE(value.hasError());
    REQUIRE(value.error().code() != ErrorCode::Ok);
}

TEST_CASE("Config getInt returns stored integer", "[config][config]")
{
    TestConfig config;
    config.set("workers", ConfigValue(std::int64_t(8)));

    const auto value = config.getInt("workers");

    REQUIRE(value.hasValue());
    REQUIRE(value.value() == 8);
}

TEST_CASE("Config getDouble returns stored double", "[config][config]")
{
    TestConfig config;
    config.set("ratio", ConfigValue(3.5));

    const auto value = config.getDouble("ratio");

    REQUIRE(value.hasValue());
    REQUIRE(value.value() == Catch::Approx(3.5));
}

TEST_CASE("Config getBool returns stored bool", "[config][config]")
{
    TestConfig config;
    config.set("enabled", ConfigValue(true));

    const auto value = config.getBool("enabled");

    REQUIRE(value.hasValue());
    REQUIRE(value.value());
}

TEST_CASE("Config getOr helpers return fallback for missing key", "[config][config]")
{
    TestConfig config;

    REQUIRE(config.getStringOr("name", "fallback") == "fallback");
    REQUIRE(config.getIntOr("workers", 4) == 4);
    REQUIRE(config.getDoubleOr("ratio", 1.25) == Catch::Approx(1.25));
    REQUIRE_FALSE(config.getBoolOr("enabled", false));
}

TEST_CASE("Config getOr helpers return stored value when key exists", "[config][config]")
{
    TestConfig config;
    config.set("name", ConfigValue(std::string("demo")));
    config.set("workers", ConfigValue(std::int64_t(16)));
    config.set("ratio", ConfigValue(2.5));
    config.set("enabled", ConfigValue(true));

    REQUIRE(config.getStringOr("name", "fallback") == "demo");
    REQUIRE(config.getIntOr("workers", 4) == 16);
    REQUIRE(config.getDoubleOr("ratio", 1.25) == Catch::Approx(2.5));
    REQUIRE(config.getBoolOr("enabled", false));
}

TEST_CASE("Config require succeeds for existing key", "[config][config]")
{
    TestConfig config;
    config.set("path", ConfigValue(std::string("/tmp/demo")));

    const auto value = config.require("path");

    REQUIRE(value.hasValue());
}

TEST_CASE("Config require returns NotFound for missing key", "[config][config]")
{
    TestConfig config;

    const auto value = config.require("path");

    REQUIRE(value.hasError());
    REQUIRE(value.error().code() == ErrorCode::NotFound);
    REQUIRE_THAT(value.error().message(), Catch::Matchers::ContainsSubstring("config path not found"));
}

TEST_CASE("Config set overwrites existing key", "[config][config]")
{
    TestConfig config;
    config.set("workers", ConfigValue(std::int64_t(4)));
    config.set("workers", ConfigValue(std::int64_t(8)));

    const auto value = config.getInt("workers");

    REQUIRE(value.hasValue());
    REQUIRE(value.value() == 8);
}

TEST_CASE("Config values exposes all inserted keys", "[config][config]")
{
    TestConfig config;
    config.set("name", ConfigValue(std::string("demo")));
    config.set("workers", ConfigValue(std::int64_t(8)));

    const auto& values = config.values();

    REQUIRE(values.size() == 2);
    REQUIRE(values.find("name") != values.end());
    REQUIRE(values.find("workers") != values.end());
}