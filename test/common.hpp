//
// Caoimheにより 2026/01/19 に作成されました。
//

#pragma once

#include <iostream>

namespace monokakido::test
{
    inline void printSeparator(const char c = '=', const size_t width = 80)
    {
        std::cout << std::string(width, c) << '\n';
    }
}