#include <iostream>
#include "dbase/math/fraction.h"

using namespace dbase;

int main()
{
    try
    {
        math::Fraction a(1, 2);
        math::Fraction b(3, 4);
        math::Fraction c(-6, 8);
        math::Fraction d(5);

        std::cout << "a = " << a << '\n';
        std::cout << "b = " << b << '\n';
        std::cout << "c = " << c << '\n';
        std::cout << "d = " << d << '\n';
        std::cout << '\n';

        std::cout << "a + b = " << (a + b) << '\n';
        std::cout << "a - b = " << (a - b) << '\n';
        std::cout << "a * b = " << (a * b) << '\n';
        std::cout << "a / b = " << (a / b) << '\n';
        std::cout << '\n';

        math::Fraction x(2, 3);
        x += math::Fraction(1, 6);
        std::cout << "x += 1/6 -> " << x << '\n';

        x -= math::Fraction(1, 2);
        std::cout << "x -= 1/2 -> " << x << '\n';

        x *= math::Fraction(9, 5);
        std::cout << "x *= 9/5 -> " << x << '\n';

        x /= math::Fraction(3, 10);
        std::cout << "x /= 3/10 -> " << x << '\n';
        std::cout << '\n';

        std::cout << std::boolalpha;
        std::cout << "a == Fraction(2, 4): " << (a == math::Fraction(2, 4)) << '\n';
        std::cout << "a < b: " << (a < b) << '\n';
        std::cout << "d > b: " << (d > b) << '\n';
        std::cout << "c = " << c << ", as double = " << c.toDouble() << '\n';
    }
    catch (const std::exception& ex)
    {
        std::cerr << "error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}