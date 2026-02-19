//
// kiwakiwaaにより 2026/02/19 に作成されました。
//

#include "monokakido/platform/directory_dictionary_source.hpp"

#include <format>

namespace monokakido
{
    DirectoryDictionarySource::DirectoryDictionarySource(fs::path root)
        : root_(std::move(root))
    {
    }


    std::expected<std::vector<DictionaryInfo>, std::string> DirectoryDictionarySource::findAllAvailable() const
    {
        if (!std::filesystem::exists(root_))
            return std::unexpected(std::format("Dictionary directory does not exist: {}", root_.string()));

        if (!std::filesystem::is_directory(root_))
            return std::unexpected(std::format("Not a directory: {}", root_.string()));

        std::vector<DictionaryInfo> result;

        for (const auto& entry : std::filesystem::directory_iterator(root_))
        {
            if (!entry.is_directory())
                continue;

            if (!looksLikeDictionary(entry.path()))
                continue;

            result.emplace_back(entry.path().filename().string(), entry.path());
        }

        return result;
    }


    std::expected<DictionaryInfo, std::string> DirectoryDictionarySource::findById(std::string_view dictId) const
    {
        const auto path = root_ / dictId;

        if (!std::filesystem::exists(path))
            return std::unexpected(std::format("Dictionary '{}' not found in {}", dictId, root_.string()));

        if (!std::filesystem::is_directory(path))
            return std::unexpected(std::format("'{}' is not a directory", path.string()));

        if (!looksLikeDictionary(path))
            return std::unexpected(std::format("'{}' does not appear to be a valid dictionary", dictId));

        return DictionaryInfo{std::string(dictId), path};
    }


    bool DirectoryDictionarySource::looksLikeDictionary(const fs::path& path)
    {
        const auto dictId = path.filename().string();
        const auto contents = path / "Contents";

        if (!fs::exists(path / "ContentInfo.plist") || !fs::is_directory(contents))
            return false;

        if (!fs::exists(contents / (dictId + ".json")))
            return false;

        return true;
    }
}
