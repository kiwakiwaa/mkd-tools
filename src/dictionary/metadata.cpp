//
// kiwakiwaaにより 2026/01/14 に作成されました。
//

#include "monokakido/core/platform/fs.hpp"
#include "monokakido/dictionary/metadata.hpp"

namespace monokakido::dictionary
{
    std::expected<DictionaryMetadata, std::string> DictionaryMetadata::loadFromPath(const fs::path& path)
    {
        auto contents = platform::fs::readTextFile(path);
        if (!contents)
        {
            return std::unexpected("Failed to read file: " + contents.error().message());
        }

        if (auto parsed = glz::lazy_json(contents.value()); !parsed)
        {
            return std::unexpected(
                "Failed to parse JSON: " + std::string(glz::format_error(parsed.error(), contents.value()))
            );
        }

        return DictionaryMetadata{std::move(contents.value())};
    }


    std::optional<std::string> DictionaryMetadata::displayName() const
    {
        const auto name = json_["DSProductDisplayName"]["ja"].get<std::string>();
        if (!name)
            return std::nullopt;

        return name.value();
    }


    std::optional<std::string> DictionaryMetadata::description() const
    {
        const auto name = json_["DSProductDescription"]["ja"].get<std::string>();
        if (!name)
            return std::nullopt;

        return name.value();
    }


    std::optional<std::string> DictionaryMetadata::publisher() const
    {
        const auto name = json_["DSProductContents"][0]["DSContentPublisher"]["ja"].get<std::string>();
        if (!name)
            return std::nullopt;

        return name.value();
    }


    std::optional<fs::path> DictionaryMetadata::contentDirectoryName() const
    {
        const auto name = json_["DSProductContents"][0]["DSContentDirectory"].get<std::string>();
        if (!name)
            return std::nullopt;

        return fs::path{name.value()};
    }


    DictionaryMetadata::DictionaryMetadata (std::string jsonContent)
        : jsonContent_(std::move(jsonContent)), json_(glz::lazy_json(jsonContent_).value())
    {
    }
}
