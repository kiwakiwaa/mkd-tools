//
// kiwakiwaaにより 2026/01/14 に作成されました。
//

#include "MKD/platform/fs.hpp"
#include "MKD/dictionary/metadata.hpp"

namespace MKD
{
    std::expected<DictionaryMetadata, std::string> DictionaryMetadata::loadFromPath(const fs::path& path)
    {
        auto fileContent = readTextFile(path);
        if (!fileContent)
            return std::unexpected("Failed to read file: " + fileContent.error().message());

        DictionaryMetadata metadata{};
        if (const auto err = glz::read<glz::opts{.error_on_unknown_keys = false}>(metadata, fileContent.value()))
            return std::unexpected("Failed to parse JSON: " + std::string(glz::format_error(err, fileContent.value())));

        return metadata;
    }
}
