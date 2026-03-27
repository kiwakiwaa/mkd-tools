//
// kiwakiwaaにより 2026/02/24 に作成されました。
//

#include "MKD/dictionary/dictionary_product.hpp"
#include "MKD/resource/resource_loader.hpp"

#include <algorithm>
#include <array>

namespace MKD
{
    namespace
    {
        std::optional<std::string> localizedValue(const std::optional<LocalizedString>& value)
        {
            if (!value)
                return std::nullopt;
            if (value->ja && !value->ja->empty())
                return value->ja;
            if (value->en && !value->en->empty())
                return value->en;
            return std::nullopt;
        }

        std::string composeSingleContentTitle(const DictionaryMetadata& metadata)
        {
            if (metadata.contents.size() != 1)
                return {};

            const auto& content = metadata.contents.front();
            const auto title = localizedValue(content.title);
            if (!title || title->empty())
                return {};

            const auto edition = localizedValue(content.edition);
            if (edition && !edition->empty())
                return *title + " " + *edition;

            return *title;
        }
    }

    Result<DictionaryProduct> DictionaryProduct::openAtPath(const fs::path& path)
    {
        auto metadata = DictionaryMetadata::loadFromPath(
            path / "Contents" / (path.stem().string() + ".json")
        );
        if (!metadata)
            return std::unexpected(std::format("Failed to load metadata: '{}'", metadata.error()));

        if (metadata->contents.empty())
            return std::unexpected("Product has no content entries");

        auto paths = DictionaryPaths::create(path);
        if (!paths)
            return std::unexpected(std::format("Failed to resolve paths: '{}'", paths.error()));

        ResourceLoader loader(*paths);
        std::vector<Dictionary> dictionaries;

        for (const auto& contentDef : metadata->contents)
        {
            const auto& dictId = contentDef.identifier;

            auto entries = loader.loadEntries(contentDef.directory, dictId);
            auto audio = loader.loadAudio(contentDef.directory, dictId);
            auto graphics = loader.loadGraphics(contentDef.directory, dictId);
            auto fonts = loader.loadFonts(contentDef.directory);
            auto keystores = loader.loadKeystores(contentDef.directory, dictId);
            auto headlines = loader.loadHeadlines(contentDef.directory);

            dictionaries.emplace_back(
                contentDef,
                std::move(entries),
                std::move(graphics),
                std::move(audio),
                std::move(fonts),
                std::move(keystores),
                std::move(headlines)
            );
        }

        return DictionaryProduct(std::move(*metadata), std::move(*paths), std::move(dictionaries));
    }


    const Dictionary* DictionaryProduct::findContent(std::string_view contentId) const
    {
        const auto it = std::ranges::find(dictionaries_, contentId, &Dictionary::id);
        return it != std::ranges::cend(dictionaries_) ? &*it : nullptr;
    }


    Dictionary* DictionaryProduct::findContent(std::string_view contentId)
    {
        const auto it = std::ranges::find(dictionaries_, contentId, &Dictionary::id);
        return it != dictionaries_.end() ? &*it : nullptr;
    }


    std::string DictionaryProduct::displayTitle() const
    {
        if (const auto singleContentTitle = composeSingleContentTitle(metadata_); !singleContentTitle.empty())
            return singleContentTitle;
        if (const auto title = localizedValue(metadata_.displayName))
            return *title;
        if (const auto title = localizedValue(metadata_.productTitle))
            return *title;
        if (!metadata_.productIdentifier.empty())
            return metadata_.productIdentifier;
        return paths_.productRoot().stem().string();
    }


    std::optional<fs::path> DictionaryProduct::iconPath() const
    {
        const auto iconsDir = paths_.productRoot() / "Contents" / "icons";
        if (!fs::is_directory(iconsDir))
            return std::nullopt;

        const auto iconBase = paths_.productRoot().stem().string();
        const std::array preferred = {
            iconsDir / (iconBase + "-76@3x.png"),
            iconsDir / (iconBase + "-76@2x.png"),
            iconsDir / (iconBase + "-76.png"),
            iconsDir / (iconBase + "-60@3x.png"),
            iconsDir / (iconBase + "-60@2x.png"),
            iconsDir / (iconBase + "-60.png"),
            iconsDir / (iconBase + "-29@3x.png"),
            iconsDir / (iconBase + "-29@2x.png"),
            iconsDir / (iconBase + "-29.png"),
            iconsDir / (iconBase + "-20@3x.png"),
            iconsDir / (iconBase + "-20@2x.png"),
            iconsDir / (iconBase + "-20.png"),
        };

        for (const auto& candidate : preferred)
        {
            if (fs::is_regular_file(candidate))
                return candidate;
        }

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(iconsDir, ec))
        {
            if (ec)
                break;
            if (entry.is_regular_file() && entry.path().extension() == ".png")
                return entry.path();
        }

        return std::nullopt;
    }


    DictionaryProduct::DictionaryProduct(DictionaryMetadata metadata, DictionaryPaths paths,
                                         std::vector<Dictionary> dictionaries)
        : metadata_(std::move(metadata)), paths_(std::move(paths)), dictionaries_(std::move(dictionaries))
    {
    }
}
