#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

#include "dbase/error/error.h"

namespace
{
using dbase::Error;
using dbase::ErrorCode;
using dbase::lastSystemErrorCode;
using dbase::makeSystemError;
using dbase::systemErrorMessage;
using dbase::toString;
}  // namespace

TEST_CASE("ErrorCode toString covers known values", "[error]")
{
    REQUIRE(std::string(toString(ErrorCode::Ok)) == "Ok");
    REQUIRE(std::string(toString(ErrorCode::InvalidArgument)) == "InvalidArgument");
    REQUIRE(std::string(toString(ErrorCode::NotFound)) == "NotFound");
    REQUIRE(std::string(toString(ErrorCode::AlreadyExists)) == "AlreadyExists");
    REQUIRE(std::string(toString(ErrorCode::IOError)) == "IOError");
    REQUIRE(std::string(toString(ErrorCode::Timeout)) == "Timeout");
    REQUIRE(std::string(toString(ErrorCode::NotSupported)) == "NotSupported");
    REQUIRE(std::string(toString(ErrorCode::SystemError)) == "SystemError");
    REQUIRE(std::string(toString(ErrorCode::ParseError)) == "ParseError");
    REQUIRE(std::string(toString(ErrorCode::InvalidState)) == "InvalidState");
    REQUIRE(std::string(toString(ErrorCode::Cancelled)) == "Cancelled");
    REQUIRE(std::string(toString(ErrorCode::WouldBlock)) == "WouldBlock");
    REQUIRE(std::string(toString(ErrorCode::EndOfFile)) == "EndOfFile");
    REQUIRE(std::string(toString(ErrorCode::Unknown)) == "Unknown");
}

TEST_CASE("Error default state is ok", "[error]")
{
    Error error;

    REQUIRE(error.ok());
    REQUIRE(error.code() == ErrorCode::Ok);
    REQUIRE(error.message().empty());
    REQUIRE(error.toString() == "Ok");
}

TEST_CASE("Error stores code and message", "[error]")
{
    Error error(ErrorCode::InvalidArgument, "bad input");

    REQUIRE_FALSE(error.ok());
    REQUIRE(error.code() == ErrorCode::InvalidArgument);
    REQUIRE(error.message() == "bad input");
    REQUIRE(error.toString() == "InvalidArgument: bad input");
}

TEST_CASE("Error toString without message returns code string only", "[error]")
{
    Error error(ErrorCode::Timeout, "");

    REQUIRE_FALSE(error.ok());
    REQUIRE(error.toString() == "Timeout");
}

TEST_CASE("systemErrorMessage returns non empty string for common system code", "[error]")
{
    const std::string message = systemErrorMessage(1);

    REQUIRE_FALSE(message.empty());
}

TEST_CASE("lastSystemErrorCode is callable", "[error]")
{
    const int code = lastSystemErrorCode();
    REQUIRE(code >= 0);
}

TEST_CASE("makeSystemError preserves prefix and uses SystemError code", "[error]")
{
    const Error error = makeSystemError("open failed", 2);

    REQUIRE_FALSE(error.ok());
    REQUIRE(error.code() == ErrorCode::SystemError);
    REQUIRE_THAT(error.message(), Catch::Matchers::ContainsSubstring("open failed"));
    REQUIRE_THAT(error.message(), Catch::Matchers::ContainsSubstring("(code=2)"));
    REQUIRE_THAT(error.toString(), Catch::Matchers::ContainsSubstring("SystemError"));
}

TEST_CASE("makeSystemError without prefix still includes system message and code", "[error]")
{
    const Error error = makeSystemError("", 2);

    REQUIRE_FALSE(error.ok());
    REQUIRE(error.code() == ErrorCode::SystemError);
    REQUIRE_THAT(error.message(), Catch::Matchers::ContainsSubstring("(code=2)"));
}