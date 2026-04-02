#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_approx.hpp>

#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "dbase/math/fraction.h"

using Catch::Matchers::ContainsSubstring;
using dbase::math::Fraction;

TEST_CASE("Fraction default constructor", "[fraction]")
{
    const Fraction f;
    REQUIRE(f.numerator() == 0);
    REQUIRE(f.denominator() == 1);
    REQUIRE(f.isZero());
    REQUIRE(f.toString() == "0");
}

TEST_CASE("Fraction integer constructor", "[fraction]")
{
    const Fraction f(5);
    REQUIRE(f.numerator() == 5);
    REQUIRE(f.denominator() == 1);
    REQUIRE_FALSE(f.isZero());
    REQUIRE(f.toString() == "5");
}

TEST_CASE("Fraction normalizes reducible fraction", "[fraction]")
{
    const Fraction f(6, 8);
    REQUIRE(f.numerator() == 3);
    REQUIRE(f.denominator() == 4);
    REQUIRE(f.toString() == "3/4");
}

TEST_CASE("Fraction normalizes negative denominator", "[fraction]")
{
    const Fraction f(1, -2);
    REQUIRE(f.numerator() == -1);
    REQUIRE(f.denominator() == 2);
    REQUIRE(f.toString() == "-1/2");
}

TEST_CASE("Fraction normalizes double negative", "[fraction]")
{
    const Fraction f(-2, -4);
    REQUIRE(f.numerator() == 1);
    REQUIRE(f.denominator() == 2);
    REQUIRE(f.toString() == "1/2");
}

TEST_CASE("Fraction zero numerator always normalized to denominator one", "[fraction]")
{
    const Fraction f(0, -99);
    REQUIRE(f.numerator() == 0);
    REQUIRE(f.denominator() == 1);
    REQUIRE(f.isZero());
    REQUIRE(f.toString() == "0");
}

TEST_CASE("Fraction unary operators", "[fraction]")
{
    const Fraction a(3, 4);
    const Fraction b = +a;
    const Fraction c = -a;

    REQUIRE(b == Fraction(3, 4));
    REQUIRE(c == Fraction(-3, 4));
}

TEST_CASE("Fraction addition", "[fraction]")
{
    const Fraction a(1, 2);
    const Fraction b(1, 3);
    const Fraction c = a + b;

    REQUIRE(c == Fraction(5, 6));
    REQUIRE(c.numerator() == 5);
    REQUIRE(c.denominator() == 6);
}

TEST_CASE("Fraction subtraction", "[fraction]")
{
    const Fraction a(3, 4);
    const Fraction b(1, 2);
    const Fraction c = a - b;

    REQUIRE(c == Fraction(1, 4));
}

TEST_CASE("Fraction multiplication", "[fraction]")
{
    const Fraction a(2, 3);
    const Fraction b(9, 10);
    const Fraction c = a * b;

    REQUIRE(c == Fraction(3, 5));
}

TEST_CASE("Fraction division", "[fraction]")
{
    const Fraction a(2, 3);
    const Fraction b(4, 5);
    const Fraction c = a / b;

    REQUIRE(c == Fraction(5, 6));
}

TEST_CASE("Fraction compound addition", "[fraction]")
{
    Fraction a(1, 2);
    a += Fraction(1, 3);
    REQUIRE(a == Fraction(5, 6));
}

TEST_CASE("Fraction compound subtraction", "[fraction]")
{
    Fraction a(5, 6);
    a -= Fraction(1, 2);
    REQUIRE(a == Fraction(1, 3));
}

TEST_CASE("Fraction compound multiplication", "[fraction]")
{
    Fraction a(3, 7);
    a *= Fraction(14, 9);
    REQUIRE(a == Fraction(2, 3));
}

TEST_CASE("Fraction compound division", "[fraction]")
{
    Fraction a(3, 5);
    a /= Fraction(9, 10);
    REQUIRE(a == Fraction(2, 3));
}

TEST_CASE("Fraction equality compares normalized values", "[fraction]")
{
    REQUIRE(Fraction(1, 2) == Fraction(2, 4));
    REQUIRE(Fraction(-3, 9) == Fraction(-1, 3));
    REQUIRE_FALSE(Fraction(1, 2) == Fraction(3, 4));
}

TEST_CASE("Fraction three way comparison with positive numbers", "[fraction]")
{
    REQUIRE(Fraction(1, 2) < Fraction(2, 3));
    REQUIRE(Fraction(3, 4) > Fraction(2, 3));
    REQUIRE(Fraction(2, 4) <= Fraction(1, 2));
    REQUIRE(Fraction(2, 4) >= Fraction(1, 2));
}

TEST_CASE("Fraction three way comparison with negative numbers", "[fraction]")
{
    REQUIRE(Fraction(-3, 4) < Fraction(-2, 3));
    REQUIRE(Fraction(-1, 2) < Fraction(0));
    REQUIRE(Fraction(0) > Fraction(-1, 100));
    REQUIRE(Fraction(-2, 4) == Fraction(-1, 2));
}

TEST_CASE("Fraction comparison with large values", "[fraction]")
{
    const auto max = std::numeric_limits<Fraction::value_type>::max();

    REQUIRE(Fraction(max - 1, max) < Fraction(1, 1));
    REQUIRE(Fraction(max - 2, max - 1) < Fraction(1, 1));
    REQUIRE(Fraction(max, max) == Fraction(1, 1));
}

TEST_CASE("Fraction toDouble", "[fraction]")
{
    const Fraction a(1, 4);
    const Fraction b(-3, 2);

    REQUIRE(a.toDouble() == Catch::Approx(0.25));
    REQUIRE(b.toDouble() == Catch::Approx(-1.5));
}

TEST_CASE("Fraction toString integer format", "[fraction]")
{
    REQUIRE(Fraction(5).toString() == "5");
    REQUIRE(Fraction(-7).toString() == "-7");
}

TEST_CASE("Fraction toString fraction format", "[fraction]")
{
    REQUIRE(Fraction(2, 3).toString() == "2/3");
    REQUIRE(Fraction(-2, 3).toString() == "-2/3");
}

TEST_CASE("Fraction stream output", "[fraction]")
{
    std::ostringstream oss1;
    oss1 << Fraction(7);
    REQUIRE(oss1.str() == "7");

    std::ostringstream oss2;
    oss2 << Fraction(-5, 8);
    REQUIRE(oss2.str() == "-5/8");
}

TEST_CASE("Fraction throws on zero denominator", "[fraction]")
{
    REQUIRE_THROWS_AS(Fraction(1, 0), std::invalid_argument);
    REQUIRE_THROWS_WITH(Fraction(1, 0), Catch::Matchers::ContainsSubstring("denominator"));
}

TEST_CASE("Fraction throws on division by zero fraction", "[fraction]")
{
    Fraction a(1, 2);
    REQUIRE_THROWS_AS(a / Fraction(0), std::domain_error);
    REQUIRE_THROWS_AS(a /= Fraction(0), std::domain_error);

    try
    {
        static_cast<void>(a / Fraction(0));
        FAIL("expected exception");
    }
    catch (const std::domain_error& ex)
    {
        REQUIRE_THAT(std::string(ex.what()), ContainsSubstring("division by zero"));
    }
}

TEST_CASE("Fraction handles zero arithmetic", "[fraction]")
{
    REQUIRE(Fraction(0) + Fraction(1, 3) == Fraction(1, 3));
    REQUIRE(Fraction(0) - Fraction(1, 3) == Fraction(-1, 3));
    REQUIRE(Fraction(0) * Fraction(5, 7) == Fraction(0));
    REQUIRE(Fraction(0) / Fraction(5, 7) == Fraction(0));
}

TEST_CASE("Fraction multiplication reduces before multiply", "[fraction]")
{
    const Fraction a(1000000, 3);
    const Fraction b(3, 1000000);
    REQUIRE(a * b == Fraction(1));
}

TEST_CASE("Fraction division reduces before multiply", "[fraction]")
{
    const Fraction a(1000000, 3);
    const Fraction b(1000000, 9);
    REQUIRE(a / b == Fraction(3));
}

TEST_CASE("Fraction add and subtract with negative values", "[fraction]")
{
    REQUIRE(Fraction(1, 2) + Fraction(-1, 3) == Fraction(1, 6));
    REQUIRE(Fraction(1, 2) - Fraction(-1, 3) == Fraction(5, 6));
    REQUIRE(Fraction(-1, 2) - Fraction(1, 3) == Fraction(-5, 6));
}

TEST_CASE("Fraction may throw on overflow sensitive operations", "[fraction]")
{
    const auto max = std::numeric_limits<Fraction::value_type>::max();
    const auto min = std::numeric_limits<Fraction::value_type>::min();

    REQUIRE_THROWS_AS(-Fraction(min, 1), std::overflow_error);
    REQUIRE_THROWS_AS(Fraction(max, 1) + Fraction(1, 1), std::overflow_error);
    REQUIRE_THROWS_AS(Fraction(max, 1) * Fraction(2, 1), std::overflow_error);
}

TEST_CASE("Fraction self operations", "[fraction]")
{
    Fraction a(2, 3);
    a += a;
    REQUIRE(a == Fraction(4, 3));

    Fraction b(3, 5);
    b *= b;
    REQUIRE(b == Fraction(9, 25));

    Fraction c(7, 9);
    c -= c;
    REQUIRE(c == Fraction(0));

    Fraction d(11, 13);
    d /= d;
    REQUIRE(d == Fraction(1));
}