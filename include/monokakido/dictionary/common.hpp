//
// kiwakiwaaにより 2026/02/19 に作成されました。
//

#pragma once

#include <filesystem>
#include <string>

namespace monokakido
{
    namespace fs = std::filesystem;

    struct DictionaryInfo
    {
        std::string id;
        std::filesystem::path path;
    };
}