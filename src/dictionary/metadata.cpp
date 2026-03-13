//
// kiwakiwaaにより 2026/01/14 に作成されました。
//

#include "MKD/dictionary/metadata.hpp"

namespace MKD
{
    Result<DictionaryMetadata> DictionaryMetadata::loadFromPath(const fs::path& path)
    {
        auto fileContent = readTextFile(path);
        if (!fileContent)
            return std::unexpected("Failed to read file: " + fileContent.error().message());

        DictionaryMetadata metadata{};
        if (const auto err = glz::read<glz::opts{.error_on_unknown_keys = false}>(metadata, fileContent.value()))
            return std::unexpected("Failed to parse JSON: " + std::string(glz::format_error(err, fileContent.value())));

        return metadata;
    }


    std::expected<std::string, std::error_code> DictionaryMetadata::readTextFile(const fs::path& path)
    {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
            return std::unexpected(ec ? ec : std::make_error_code(std::errc::no_such_file_or_directory));

        if (std::filesystem::is_directory(path))
            return std::unexpected(ec ? ec : std::make_error_code(std::errc::is_a_directory));

        const auto fileSize = std::filesystem::file_size(path, ec);
        if (ec)
            return std::unexpected(ec);

        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file)
        {
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }

        std::string content;
        content.resize(fileSize);

        if (!file.read(content.data(), static_cast<long>(fileSize)))
        {
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }

        return content;
    }
}
