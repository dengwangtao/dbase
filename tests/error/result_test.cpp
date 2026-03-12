#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <utility>

#include "dbase/error/error.h"

namespace
{
using dbase::BadResultAccess;
using dbase::Error;
using dbase::ErrorCode;
using dbase::makeError;
using dbase::makeErrorResult;
using dbase::makeResult;
using dbase::makeSystemError;
using dbase::makeSystemErrorResult;
using dbase::makeSystemErrorResultT;
using dbase::Result;

struct Payload
{
        int value{0};
        std::string text;
};
}  // namespace

TEST_CASE("Error default state is ok", "[error][result]")
{
    Error error;

    REQUIRE(error.ok());
    REQUIRE(error.code() == ErrorCode::Ok);
    REQUIRE(error.message().empty());
    REQUIRE_FALSE(error.toString().empty());
}

TEST_CASE("Error stores code and message", "[error][result]")
{
    Error error(ErrorCode::InvalidArgument, "bad input");

    REQUIRE_FALSE(error.ok());
    REQUIRE(error.code() == ErrorCode::InvalidArgument);
    REQUIRE(error.message() == "bad input");
    REQUIRE_THAT(error.toString(), Catch::Matchers::ContainsSubstring("bad input"));
}

TEST_CASE("Result T constructed from value has value", "[error][result]")
{
    Result<int> result(42);

    REQUIRE(result.hasValue());
    REQUIRE_FALSE(result.hasError());
    REQUIRE(static_cast<bool>(result));
    REQUIRE(result.value() == 42);
    REQUIRE(*result == 42);
}

TEST_CASE("Result T constructed from error has error", "[error][result]")
{
    Result<int> result(Error(ErrorCode::NotFound, "missing"));

    REQUIRE_FALSE(result.hasValue());
    REQUIRE(result.hasError());
    REQUIRE_FALSE(static_cast<bool>(result));
    REQUIRE(result.error().code() == ErrorCode::NotFound);
    REQUIRE(result.error().message() == "missing");
}

TEST_CASE("Result T value throws BadResultAccess on error result", "[error][result]")
{
    Result<int> result(Error(ErrorCode::SystemError, "boom"));

    REQUIRE_THROWS_AS(result.value(), BadResultAccess);
    REQUIRE_THROWS_WITH(result.value(), Catch::Matchers::ContainsSubstring("Result::value() called on error result"));
}

TEST_CASE("Result T error throws BadResultAccess on value result", "[error][result]")
{
    Result<int> result(7);

    REQUIRE_THROWS_AS(result.error(), BadResultAccess);
    REQUIRE_THROWS_WITH(result.error(), Catch::Matchers::ContainsSubstring("Result::error() called on value result"));
}

TEST_CASE("Result T valueOr returns held value when present", "[error][result]")
{
    Result<int> result(123);

    REQUIRE(result.valueOr(999) == 123);
}

TEST_CASE("Result T valueOr returns default when result has error", "[error][result]")
{
    Result<int> result(Error(ErrorCode::Timeout, "timeout"));

    REQUIRE(result.valueOr(999) == 999);
}

TEST_CASE("Result T operator arrow accesses underlying object", "[error][result]")
{
    Result<Payload> result(Payload{7, "hello"});

    REQUIRE(result->value == 7);
    REQUIRE(result->text == "hello");
    REQUIRE((*result).text == "hello");
}

TEST_CASE("Result T supports move value access on rvalue", "[error][result]")
{
    Result<std::string> result(std::string("abc"));

    std::string moved = std::move(result).value();

    REQUIRE(moved == "abc");
}

TEST_CASE("Result void default constructed has value", "[error][result]")
{
    Result<void> result;

    REQUIRE(result.hasValue());
    REQUIRE_FALSE(result.hasError());
    REQUIRE(static_cast<bool>(result));
    REQUIRE_NOTHROW(result.value());
}

TEST_CASE("Result void constructed from error has error", "[error][result]")
{
    Result<void> result(Error(ErrorCode::InvalidState, "bad state"));

    REQUIRE_FALSE(result.hasValue());
    REQUIRE(result.hasError());
    REQUIRE_FALSE(static_cast<bool>(result));
    REQUIRE(result.error().code() == ErrorCode::InvalidState);
    REQUIRE(result.error().message() == "bad state");
}

TEST_CASE("Result void value throws on error result", "[error][result]")
{
    Result<void> result(Error(ErrorCode::SystemError, "oops"));

    REQUIRE_THROWS_AS(result.value(), BadResultAccess);
    REQUIRE_THROWS_WITH(result.value(), Catch::Matchers::ContainsSubstring("Result<void>::value() called on error result"));
}

TEST_CASE("Result void error throws on value result", "[error][result]")
{
    Result<void> result;

    REQUIRE_THROWS_AS(result.error(), BadResultAccess);
    REQUIRE_THROWS_WITH(result.error(), Catch::Matchers::ContainsSubstring("Result<void>::error() called on value result"));
}

TEST_CASE("makeResult creates Result from forwarded value", "[error][result]")
{
    auto result = makeResult(std::string("hello"));

    REQUIRE(result.hasValue());
    REQUIRE(result.value() == "hello");
}

TEST_CASE("makeError creates Result void with error", "[error][result]")
{
    auto result = makeError(ErrorCode::NotFound, "missing");

    REQUIRE_FALSE(result.hasValue());
    REQUIRE(result.hasError());
    REQUIRE(result.error().code() == ErrorCode::NotFound);
    REQUIRE(result.error().message() == "missing");
}

TEST_CASE("makeErrorResult creates typed error result", "[error][result]")
{
    auto result = makeErrorResult<int>(ErrorCode::Timeout, "late");

    REQUIRE_FALSE(result.hasValue());
    REQUIRE(result.hasError());
    REQUIRE(result.error().code() == ErrorCode::Timeout);
    REQUIRE(result.error().message() == "late");
}

TEST_CASE("makeSystemError creates error with supplied system code", "[error][result]")
{
    auto error = makeSystemError("syscall failed", 123);

    REQUIRE_FALSE(error.ok());
    REQUIRE(error.code() == ErrorCode::SystemError);
    REQUIRE_THAT(error.message(), Catch::Matchers::ContainsSubstring("syscall failed"));
}

TEST_CASE("makeSystemErrorResult creates void error result", "[error][result]")
{
    auto result = makeSystemErrorResult("io failed", 456);

    REQUIRE_FALSE(result.hasValue());
    REQUIRE(result.hasError());
    REQUIRE(result.error().code() == ErrorCode::SystemError);
    REQUIRE_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("io failed"));
}

TEST_CASE("makeSystemErrorResultT creates typed system error result", "[error][result]")
{
    auto result = makeSystemErrorResultT<int>("network failed", 789);

    REQUIRE_FALSE(result.hasValue());
    REQUIRE(result.hasError());
    REQUIRE(result.error().code() == ErrorCode::SystemError);
    REQUIRE_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("network failed"));
}

TEST_CASE("Result preserves exact error object", "[error][result]")
{
    Error error(ErrorCode::AlreadyExists, "duplicate");
    Result<int> result(error);

    REQUIRE(result.hasError());
    REQUIRE(result.error().code() == ErrorCode::AlreadyExists);
    REQUIRE(result.error().message() == "duplicate");
}

TEST_CASE("Result void preserves exact error object", "[error][result]")
{
    Error error(ErrorCode::Cancelled, "cancelled");
    Result<void> result(error);

    REQUIRE(result.hasError());
    REQUIRE(result.error().code() == ErrorCode::Cancelled);
    REQUIRE(result.error().message() == "cancelled");
}