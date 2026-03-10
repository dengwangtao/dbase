#include "dbase/str/str.h"
#include "dbase/time/time.h"
#include "dbase/platform/process.h"

#include <iostream>

int main()
{
    std::cout << dbase::str::trim("  hello dbase  ") << '\n';
    std::cout << dbase::time::formatNow() << '\n';
    std::cout << dbase::platform::pid() << '\n';
    std::cout << dbase::platform::tid() << '\n';

    const auto exe = dbase::platform::executablePath();
    if (exe)
    {
        std::cout << exe.value().string() << '\n';
    }
    else
    {
        std::cout << exe.error().message() << '\n';
    }

    return 0;
}