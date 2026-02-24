//
// kiwakiwaaにより 2026/02/24 に作成されました。
//

#include "MKD/dictionary/dictionary_product.hpp"
#include "MKD/resource/resource_loader.hpp"

namespace MKD
{
    std::expected<DictionaryProduct, std::string> DictionaryProduct::openAtPath(const fs::path& path)
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


    DictionaryProduct::DictionaryProduct(DictionaryMetadata metadata, DictionaryPaths paths,
                                         std::vector<Dictionary> dictionaries)
        : metadata_(std::move(metadata)), paths_(std::move(paths)), dictionaries_(std::move(dictionaries))
    {
    }
}
