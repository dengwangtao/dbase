#pragma once

#include <compare>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace dbase::math
{

class Fraction
{
    public:
        using value_type = std::int64_t;

        Fraction() noexcept;
        Fraction(value_type numerator) noexcept;
        Fraction(value_type numerator, value_type denominator);

        value_type numerator() const noexcept;
        value_type denominator() const noexcept;

        bool isZero() const noexcept;
        double toDouble() const noexcept;
        std::string toString() const;

        Fraction operator+() const noexcept;
        Fraction operator-() const;

        Fraction& operator+=(const Fraction& other);
        Fraction& operator-=(const Fraction& other);
        Fraction& operator*=(const Fraction& other);
        Fraction& operator/=(const Fraction& other);

        friend Fraction operator+(Fraction lhs, const Fraction& rhs);
        friend Fraction operator-(Fraction lhs, const Fraction& rhs);
        friend Fraction operator*(Fraction lhs, const Fraction& rhs);
        friend Fraction operator/(Fraction lhs, const Fraction& rhs);

        friend bool operator==(const Fraction& lhs, const Fraction& rhs) noexcept;
        friend std::strong_ordering operator<=>(const Fraction& lhs, const Fraction& rhs) noexcept;

        friend std::ostream& operator<<(std::ostream& os, const Fraction& fraction);

    private:
        void normalize();

        static std::uint64_t absToUint64(value_type x) noexcept;
        static value_type safeNegate(value_type x);
        static value_type safeAdd(value_type a, value_type b);
        static value_type safeSub(value_type a, value_type b);
        static value_type safeMul(value_type a, value_type b);
        static value_type safeCastU64ToI64(std::uint64_t x);
        static std::uint64_t gcdU64(std::uint64_t a, std::uint64_t b) noexcept;

        static std::strong_ordering compareUnsignedFractions(std::uint64_t a,
                                                             std::uint64_t b,
                                                             std::uint64_t c,
                                                             std::uint64_t d) noexcept;

    private:
        value_type num_;
        value_type den_;
};

}  // namespace math