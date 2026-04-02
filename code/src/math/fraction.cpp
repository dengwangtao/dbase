#include "dbase/math/fraction.h"

#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace dbase::math
{

Fraction::Fraction() noexcept : num_(0), den_(1)
{
}

Fraction::Fraction(value_type numerator) noexcept : num_(numerator), den_(1)
{
}

Fraction::Fraction(value_type numerator, value_type denominator) : num_(numerator), den_(denominator)
{
    normalize();
}

Fraction::value_type Fraction::numerator() const noexcept
{
    return num_;
}

Fraction::value_type Fraction::denominator() const noexcept
{
    return den_;
}

bool Fraction::isZero() const noexcept
{
    return num_ == 0;
}

double Fraction::toDouble() const noexcept
{
    return static_cast<double>(num_) / static_cast<double>(den_);
}

std::string Fraction::toString() const
{
    std::ostringstream oss;
    oss << *this;
    return oss.str();
}

Fraction Fraction::operator+() const noexcept
{
    return *this;
}

Fraction Fraction::operator-() const
{
    return Fraction(safeNegate(num_), den_);
}

Fraction& Fraction::operator+=(const Fraction& other)
{
    const std::uint64_t g_u = gcdU64(absToUint64(den_), absToUint64(other.den_));
    const value_type g = safeCastU64ToI64(g_u);

    const value_type left_mul = other.den_ / g;
    const value_type right_mul = den_ / g;

    const value_type new_num = safeAdd(safeMul(num_, left_mul), safeMul(other.num_, right_mul));
    const value_type new_den = safeMul(den_, left_mul);

    num_ = new_num;
    den_ = new_den;
    normalize();
    return *this;
}

Fraction& Fraction::operator-=(const Fraction& other)
{
    const std::uint64_t g_u = gcdU64(absToUint64(den_), absToUint64(other.den_));
    const value_type g = safeCastU64ToI64(g_u);

    const value_type left_mul = other.den_ / g;
    const value_type right_mul = den_ / g;

    const value_type new_num = safeSub(safeMul(num_, left_mul), safeMul(other.num_, right_mul));
    const value_type new_den = safeMul(den_, left_mul);

    num_ = new_num;
    den_ = new_den;
    normalize();
    return *this;
}

Fraction& Fraction::operator*=(const Fraction& other)
{
    if (num_ == 0 || other.num_ == 0)
    {
        num_ = 0;
        den_ = 1;
        return *this;
    }

    value_type a = num_;
    value_type b = den_;
    value_type c = other.num_;
    value_type d = other.den_;

    const value_type g1 = safeCastU64ToI64(gcdU64(absToUint64(a), absToUint64(d)));
    a /= g1;
    d /= g1;

    const value_type g2 = safeCastU64ToI64(gcdU64(absToUint64(c), absToUint64(b)));
    c /= g2;
    b /= g2;

    num_ = safeMul(a, c);
    den_ = safeMul(b, d);
    normalize();
    return *this;
}

Fraction& Fraction::operator/=(const Fraction& other)
{
    if (other.num_ == 0)
    {
        throw std::domain_error("division by zero fraction");
    }

    if (num_ == 0)
    {
        den_ = 1;
        return *this;
    }

    value_type a = num_;
    value_type b = den_;
    value_type c = other.num_;
    value_type d = other.den_;

    const value_type g1 = safeCastU64ToI64(gcdU64(absToUint64(a), absToUint64(c)));
    a /= g1;
    c /= g1;

    const value_type g2 = safeCastU64ToI64(gcdU64(absToUint64(d), absToUint64(b)));
    d /= g2;
    b /= g2;

    num_ = safeMul(a, d);
    den_ = safeMul(b, c);
    normalize();
    return *this;
}

Fraction operator+(Fraction lhs, const Fraction& rhs)
{
    lhs += rhs;
    return lhs;
}

Fraction operator-(Fraction lhs, const Fraction& rhs)
{
    lhs -= rhs;
    return lhs;
}

Fraction operator*(Fraction lhs, const Fraction& rhs)
{
    lhs *= rhs;
    return lhs;
}

Fraction operator/(Fraction lhs, const Fraction& rhs)
{
    lhs /= rhs;
    return lhs;
}

bool operator==(const Fraction& lhs, const Fraction& rhs) noexcept
{
    return lhs.num_ == rhs.num_ && lhs.den_ == rhs.den_;
}

std::strong_ordering operator<=>(const Fraction& lhs, const Fraction& rhs) noexcept
{
    if (lhs.num_ == rhs.num_ && lhs.den_ == rhs.den_)
    {
        return std::strong_ordering::equal;
    }

    if (lhs.num_ < 0 && rhs.num_ >= 0)
    {
        return std::strong_ordering::less;
    }

    if (lhs.num_ >= 0 && rhs.num_ < 0)
    {
        return std::strong_ordering::greater;
    }

    const std::uint64_t lhs_abs = Fraction::absToUint64(lhs.num_);
    const std::uint64_t rhs_abs = Fraction::absToUint64(rhs.num_);

    const std::strong_ordering ord =
            Fraction::compareUnsignedFractions(lhs_abs,
                                               static_cast<std::uint64_t>(lhs.den_),
                                               rhs_abs,
                                               static_cast<std::uint64_t>(rhs.den_));

    if (lhs.num_ < 0)
    {
        if (ord == std::strong_ordering::less)
        {
            return std::strong_ordering::greater;
        }
        if (ord == std::strong_ordering::greater)
        {
            return std::strong_ordering::less;
        }
    }

    return ord;
}

std::ostream& operator<<(std::ostream& os, const Fraction& fraction)
{
    if (fraction.den_ == 1)
    {
        os << fraction.num_;
    }
    else
    {
        os << fraction.num_ << '/' << fraction.den_;
    }
    return os;
}

void Fraction::normalize()
{
    if (den_ == 0)
    {
        throw std::invalid_argument("denominator cannot be zero");
    }

    if (num_ == 0)
    {
        den_ = 1;
        return;
    }

    const std::uint64_t g_u = gcdU64(absToUint64(num_), absToUint64(den_));
    const value_type g = safeCastU64ToI64(g_u);

    num_ /= g;
    den_ /= g;

    if (den_ < 0)
    {
        if (den_ == std::numeric_limits<value_type>::min())
        {
            throw std::overflow_error("fraction normalization overflow");
        }

        num_ = safeNegate(num_);
        den_ = -den_;
    }
}

std::uint64_t Fraction::absToUint64(value_type x) noexcept
{
    if (x >= 0)
    {
        return static_cast<std::uint64_t>(x);
    }
    return static_cast<std::uint64_t>(-(x + 1)) + 1;
}

Fraction::value_type Fraction::safeNegate(value_type x)
{
    if (x == std::numeric_limits<value_type>::min())
    {
        throw std::overflow_error("integer overflow on negate");
    }
    return -x;
}

Fraction::value_type Fraction::safeAdd(value_type a, value_type b)
{
    if (b > 0 && a > std::numeric_limits<value_type>::max() - b)
    {
        throw std::overflow_error("integer overflow on add");
    }

    if (b < 0 && a < std::numeric_limits<value_type>::min() - b)
    {
        throw std::overflow_error("integer overflow on add");
    }

    return a + b;
}

Fraction::value_type Fraction::safeSub(value_type a, value_type b)
{
    if (b == std::numeric_limits<value_type>::min())
    {
        throw std::overflow_error("integer overflow on sub");
    }
    return safeAdd(a, -b);
}

Fraction::value_type Fraction::safeMul(value_type a, value_type b)
{
    if (a == 0 || b == 0)
    {
        return 0;
    }

    if (a == 1)
    {
        return b;
    }

    if (b == 1)
    {
        return a;
    }

    if (a == -1)
    {
        return safeNegate(b);
    }

    if (b == -1)
    {
        return safeNegate(a);
    }

    if (a > 0)
    {
        if (b > 0)
        {
            if (a > std::numeric_limits<value_type>::max() / b)
            {
                throw std::overflow_error("integer overflow on mul");
            }
        }
        else
        {
            if (b < std::numeric_limits<value_type>::min() / a)
            {
                throw std::overflow_error("integer overflow on mul");
            }
        }
    }
    else
    {
        if (b > 0)
        {
            if (a < std::numeric_limits<value_type>::min() / b)
            {
                throw std::overflow_error("integer overflow on mul");
            }
        }
        else
        {
            if (a < std::numeric_limits<value_type>::max() / b)
            {
                throw std::overflow_error("integer overflow on mul");
            }
        }
    }

    return a * b;
}

Fraction::value_type Fraction::safeCastU64ToI64(std::uint64_t x)
{
    if (x == 0)
    {
        return 0;
    }

    if (x > static_cast<std::uint64_t>(std::numeric_limits<value_type>::max()))
    {
        throw std::overflow_error("integer overflow on cast");
    }

    return static_cast<value_type>(x);
}

std::uint64_t Fraction::gcdU64(std::uint64_t a, std::uint64_t b) noexcept
{
    while (b != 0)
    {
        const std::uint64_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

std::strong_ordering Fraction::compareUnsignedFractions(std::uint64_t a,
                                                        std::uint64_t b,
                                                        std::uint64_t c,
                                                        std::uint64_t d) noexcept
{
    bool reversed = false;

    for (;;)
    {
        const std::uint64_t q1 = a / b;
        const std::uint64_t q2 = c / d;

        if (q1 != q2)
        {
            const bool less = q1 < q2;
            if (!reversed)
            {
                return less ? std::strong_ordering::less : std::strong_ordering::greater;
            }
            return less ? std::strong_ordering::greater : std::strong_ordering::less;
        }

        const std::uint64_t r1 = a % b;
        const std::uint64_t r2 = c % d;

        if (r1 == 0 || r2 == 0)
        {
            if (r1 == 0 && r2 == 0)
            {
                return std::strong_ordering::equal;
            }

            const bool less = (r1 == 0);
            if (!reversed)
            {
                return less ? std::strong_ordering::less : std::strong_ordering::greater;
            }
            return less ? std::strong_ordering::greater : std::strong_ordering::less;
        }

        a = b;
        b = r1;
        c = d;
        d = r2;
        reversed = !reversed;
    }
}

}  // namespace dbase::math