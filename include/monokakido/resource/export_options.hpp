//
// Caoimheにより 2026/01/17 に作成されました。
//

#pragma once

#include <filesystem>

namespace fs = std::filesystem;

namespace monokakido::resource
{

    enum class NamingStrategy
    {
        PreserveId,
        NormaliseUnicode,
        Sequential
    };

    struct ExportOptions
    {
        fs::path outputDirectory;
        NamingStrategy naming = NamingStrategy::PreserveId;
        bool overwriteExisting = false;
        bool createDirectories = true;
    };

}