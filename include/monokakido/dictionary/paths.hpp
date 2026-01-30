//
// kiwakiwaaにより 2026/01/14 に作成されました。
//

#pragma once

#include "metadata.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace fs = std::filesystem;

namespace monokakido::dictionary
{

    enum class PathType
    {
        Contents,
        Graphics,
        Audio,
        Headline,
        Keystore,
        Appendix
    };

    class DictionaryPaths
    {
    public:

        static std::expected<DictionaryPaths, std::string> create(const fs::path& rootPath, const DictionaryMetadata& metadata);

        [[nodiscard]] fs::path resolve(PathType type) const;
        [[nodiscard]] std::optional<fs::path> tryResolve(PathType type) const;
        [[nodiscard]] std::expected<fs::path, std::string> validate(PathType type) const;

    private:

        DictionaryPaths(fs::path rootPath, fs::path contentDirectory);

        fs::path rootPath_;
        fs::path contentDirectory_;

    };

}
