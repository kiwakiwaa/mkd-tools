//
// kiwakiwaaにより 2026/01/15 に作成されました。
//


#include "monokakido/dictionary/dictionary.hpp"
#include "monokakido/dictionary/catalog.hpp"
#include "monokakido/resource/resource_loader.hpp"

#include <utility>

namespace monokakido
{
    std::expected<Dictionary, std::string> Dictionary::open(std::string_view dictId)
    {
        const auto& catalog = DictionaryCatalog::instance();

        const auto dictInfo = catalog.findById(dictId);
        if (!dictInfo)
            return std::unexpected(std::format("Dictionary '{}' not found", dictId));

        return openAtPath(dictInfo->path);
    }


    std::expected<Dictionary, std::string> Dictionary::openAtPath(const fs::path& path)
    {
        auto dictId = path.stem().string();

        auto metadata = DictionaryMetadata::loadFromPath(
            path / "Contents" / (dictId + ".json")
        );
        if (!metadata)
            return std::unexpected(std::format("Failed to load dictionary metadata: '{}'", metadata.error()));

        auto paths = DictionaryPaths::create(path, metadata.value());
        if (!paths)
            return std::unexpected(std::format("Failed to load paths: '{}'", paths.error()));

        ResourceLoader loader(*paths);

        auto entries = loader.loadEntries();
        auto audio = loader.loadAudio();
        auto graphics = loader.loadGraphics();
        auto fonts = loader.loadFonts();

        return Dictionary(
            std::string(dictId),
            std::move(metadata.value()),
            std::move(paths.value()),
            std::move(entries),
            std::move(graphics),
            std::move(audio),
            std::move(fonts)
        );
    }


    const std::string& Dictionary::id() const noexcept
    {
        return id_;
    }


    const DictionaryMetadata& Dictionary::metadata() const noexcept
    {
        return metadata_;
    }


    Nrsc* Dictionary::graphics() noexcept
    {
        if (!graphics_ || graphics_->empty())
            return nullptr;

        return &*graphics_;
    }


    const Nrsc* Dictionary::graphics() const noexcept
    {
        if (!graphics_ || graphics_->empty())
            return nullptr;

        return &*graphics_;
    }


    std::vector<Font>& Dictionary::fonts() noexcept
    {
        return fonts_;
    }


    const std::vector<Font>& Dictionary::fonts() const noexcept
    {
        return fonts_;
    }


    bool Dictionary::hasAudio() const noexcept
    {
        return audio_.has_value();
    }


    bool Dictionary::hasGraphics() const noexcept
    {
        return graphics_ && !graphics_->empty();
    }


    bool Dictionary::hasFonts() const noexcept
    {
        return !fonts_.empty();
    }


    Dictionary::Dictionary(std::string id,
                           DictionaryMetadata metadata,
                           DictionaryPaths paths,
                           std::optional<Rsc> entries,
                           std::optional<Nrsc> graphics,
                           std::optional<std::variant<Rsc, Nrsc>> audio,
                           std::vector<Font> fonts)
        : id_(std::move(id)), paths_(std::move(paths)), metadata_(std::move(metadata)),
          entries_(std::move(entries)),
          graphics_(std::move(graphics)),
          audio_(std::move(audio)),
          fonts_(std::move(fonts))
    {
    }


    ExportResult Dictionary::exportAll(const ExportOptions& options) const
    {
        ExportResult combinedResult;

        if (hasFonts())
        {
            if (auto result = exportFonts(options))
                combinedResult += *result;
            else
                combinedResult.errors.push_back(std::format("Failed to export fonts: {}", result.error()));
        }

        if (hasGraphics())
        {
            if (auto result = exportGraphics(options))
                combinedResult += *result;
            else
                combinedResult.errors.push_back(std::format("Failed to export graphics: {}", result.error()));
        }

        if (hasAudio())
        {
            if (auto result = exportAudio(options))
                combinedResult += *result;
            else
                combinedResult.errors.push_back(std::format("Failed to export audio: {}", result.error()));
        }

        if (entries_)
        {
            if (auto result = exportEntries(options))
                combinedResult += *result;
            else
                combinedResult.errors.push_back(std::format("Failed to export entries: {}", result.error()));
        }

        return combinedResult;
    }


    std::expected<ExportResult, std::string> Dictionary::exportAudio(const ExportOptions& options) const
    {
        if (!hasAudio())
            return ExportResult{};

        return std::visit([&options](const auto& audioResource) {
            return ResourceExporter::exportAll(audioResource, options);
        }, *audio_);
    }


    std::expected<ExportResult, std::string> Dictionary::exportEntries(const ExportOptions& options) const
    {
        if (!entries_)
            return ExportResult{};

        return ResourceExporter::exportAll(*entries_, options);
    }


    std::expected<ExportResult, std::string> Dictionary::exportGraphics(
        const ExportOptions& options) const
    {
        return hasGraphics() ? ResourceExporter::exportAll(*graphics_, options) : ExportResult{};
    }


    std::expected<ExportResult, std::string> Dictionary::exportFonts(
        const ExportOptions& options) const
    {
        ExportResult combinedResult;
        for (auto& font : fonts_)
        {
            auto result = ResourceExporter::exportFont(font, options);
            if (!result)
                return result;

            combinedResult += *result;
        }
        return combinedResult;
    }
}
