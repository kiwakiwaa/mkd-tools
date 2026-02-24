//
// kiwakiwaaにより 2026/01/15 に作成されました。
//


#include "MKD/dictionary/dictionary.hpp"
#include "MKD/resource/resource_loader.hpp"

#include <utility>

namespace MKD
{
    Dictionary::Dictionary(DictionaryContent content,
                           std::optional<Rsc> entries,
                           std::optional<Nrsc> graphics,
                           std::optional<std::variant<Rsc, Nrsc> > audio,
                           std::vector<Font> fonts,
                           std::vector<Keystore> keystores,
                           std::vector<HeadlineStore> headlines)
        : content_(std::move(content)),
          entries_(std::move(entries)),
          graphics_(std::move(graphics)),
          audio_(std::move(audio)),
          fonts_(std::move(fonts)),
          keystores_(std::move(keystores)),
          headlines_(std::move(headlines))
    {
    }


    const std::string& Dictionary::id() const noexcept
    {
        return content_.identifier;
    }


    const DictionaryContent& Dictionary::content() const noexcept
    {
        return content_;
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

    size_t Dictionary::resourceCount(const ResourceType type) const noexcept
    {
        switch (type)
        {
            case ResourceType::Audio:
                return audio_ ? std::visit([](const auto& r) { return r.size(); }, *audio_) : 0;
            case ResourceType::Entries:
                return entries_ ? entries_->size() : 0;
            case ResourceType::Graphics:
                return graphics_ ? graphics_->size() : 0;
            case ResourceType::Fonts:
                return fonts_.size();
            case ResourceType::Keystores:
                return keystores_.size();
            case ResourceType::Headlines:
            {
                size_t total = 0;
                for (const auto& store : headlines_)
                    total += store.size();
                return total;
            }
        }
        std::unreachable();
    }


    ExportResult Dictionary::exportWithOptions(const ExportOptions& options) const
    {
        ExportResult combinedResult;

        const auto shouldExport = [&](const ResourceType type) {
            return options.resources.empty()
                   || std::ranges::find(options.resources, type) != options.resources.end();
        };

        const auto notify = [&](const ExportEvent& event) {
            if (options.progressCallback)
                options.progressCallback(event);
        };

        // Pre-count heavy resource totals for the unified progress bar
        size_t heavyTotal = 0;
        for (const auto type : {ResourceType::Audio, ResourceType::Entries, ResourceType::Graphics})
        {
            if (shouldExport(type))
                heavyTotal += resourceCount(type);
        }

        notify(ExportBeginEvent{.totalItems = heavyTotal});

        const auto runPhase = [&](const ResourceType type, auto exportFn) {
            if (!shouldExport(type))
                return;

            const size_t totalItems = resourceCount(type);
            if (totalItems == 0)
                return;

            notify(PhaseBeginEvent{type, totalItems});

            if (auto result = exportFn())
            {
                notify(PhaseEndEvent{type, *result});
                combinedResult += *result;
            }
            else
            {
                ExportResult failed;
                failed.failed = 1;
                failed.errors.push_back(std::format("Failed to export {}: {}",
                                                    resourceTypeName(type), result.error()));
                notify(PhaseEndEvent{type, failed});
                combinedResult += failed;
            }
        };

        runPhase(ResourceType::Audio, [&] { return exportAudio(options); });
        runPhase(ResourceType::Entries, [&] {
            return ResourceExporter::exportAll(*entries_, options, ResourceType::Entries);
        });
        runPhase(ResourceType::Fonts, [&] { return exportFonts(options); });
        runPhase(ResourceType::Graphics, [&] {
            return ResourceExporter::exportAll(*graphics_, options, ResourceType::Graphics);
        });
        runPhase(ResourceType::Headlines, [&] { return exportHeadlines(options); });
        runPhase(ResourceType::Keystores, [&] { return exportKeystores(options); });

        return combinedResult;
    }


    std::expected<ExportResult, std::string> Dictionary::exportAudio(const ExportOptions& options) const
    {
        return std::visit([&options](const auto& audioResource) {
            return ResourceExporter::exportAll(audioResource, options, ResourceType::Audio);
        }, *audio_);
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


    std::expected<ExportResult, std::string> Dictionary::exportKeystores(const ExportOptions& options) const
    {
        ExportResult combined;
        for (const auto& keystore : keystores_)
        {
            auto result = KeystoreExporter::exportKeystore(keystore, options);
            if (!result)
                return result;

            combined += *result;
        }
        return combined;
    }


    std::expected<ExportResult, std::string> Dictionary::exportHeadlines(const ExportOptions& options) const
    {
        ExportResult combined;
        for (const auto& store : headlines_)
        {
            auto result = HeadlineExporter::exportHeadlines(store, options);
            if (!result)
                return result;

            combined += *result;
        }
        return combined;
    }
}
