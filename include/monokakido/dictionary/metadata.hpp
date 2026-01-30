//
// kiwakiwaaにより 2026/01/14 に作成されました。
//

#pragma once

#include <glaze/glaze.hpp>

#include <filesystem>
#include <optional>

namespace fs = std::filesystem;

namespace monokakido::dictionary
{

    class DictionaryMetadata
    {
    public:
        static std::expected<DictionaryMetadata, std::string> loadFromPath(const fs::path& path);

        [[nodiscard]] std::optional<std::string> displayName() const;
        [[nodiscard]] std::optional<std::string> description() const;
        [[nodiscard]] std::optional<std::string> publisher() const;

        [[nodiscard]] std::optional<fs::path> contentDirectoryName() const;

    private:

        explicit DictionaryMetadata(std::string jsonContent);

        std::string jsonContent_;
        glz::lazy_document<> json_;

    };
}