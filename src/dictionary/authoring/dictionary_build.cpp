#include "dictionary/content/document_checks.hpp"
#include "dictionary/package/paths.hpp"
//
// kiwakiwaaにより 2026/10/24 に作成されました。
//

#include "dictionary_build.hpp"
#include "product_build.hpp"
#include "MKD/dictionary/content_analysis.hpp"
#include "unicode/utf8.hpp"
#include "dictionary/content/content_markup.hpp"
#include "dictionary/search/search_definitions.hpp"
#include "dictionary/authoring/metadata_preparation.hpp"
#include "dictionary/package/metadata_writer.hpp"
#include "io/write_validation.hpp"
#include "resource/detail/font_format.hpp"
#include "output/directory_output.hpp"
#include <algorithm>

namespace MKD::detail
{
    DictionaryBuild::DictionaryBuild(std::shared_ptr<ProductBuild> product, DictionaryContent metadata,
                                     DictionaryWriteOptions options, SourceContext context)
        : product_(product), metadata_(std::move(metadata)),
          root_(product->path() / monokakido::paths::productContents / WriteValidation::fromUtf8(metadata_.directory)),
          metadataContext_(std::move(context)), state_(std::in_place_type<Working>, root_, product->profile(), options)
    {
    }

    Result<std::shared_ptr<ProductBuild>> DictionaryBuild::owner() const
    {
        auto product = product_.lock();
        if (!product)
            return std::unexpected("Product writer no longer exists");

        if (auto state = product->check(); !state)
            return std::unexpected(state.error());

        if (finished())
            return std::unexpected("Dictionary has already finished");

        return product;
    }

    void DictionaryBuild::fail(std::string error) const
    {
        if (auto product = product_.lock())
            (void)product->fail(std::move(error));
    }

    Result<uint32_t> DictionaryBuild::reserveEntry(std::optional<uint32_t> page, SourceContext context)
    {
        const auto source = origin("reserveEntry", page ? std::to_string(*page) : "automatic page", context);
        return mutate([&]() -> Result<uint32_t>
        {
            auto reserved = working().entries.reserve(page);
            if (reserved)
                working().reservations.emplace(*reserved, source);
            return reserved;
        }, source);
    }

    Result<EntryId> DictionaryBuild::addTarget(const uint32_t page, const EntryTargetKind kind, SourceContext context)
    {
        const auto source = origin("addTarget", std::to_string(page), context);
        return mutate([&]() -> Result<EntryId>
        {
            auto target = working().entries.addTarget(page, kind);
            if (target)
                working().references.requireTarget(*target, source);

            return target;
        }, source);
    }

    Result<void> DictionaryBuild::addPage(const uint32_t page, std::string_view xml, SourceContext context)
    {
        const auto source = origin("addPage", "page " + std::to_string(page), context);
        return mutate([&]() -> Result<void>
        {
            if (working().entries.reserved(page))
                return std::unexpected("Page is reserved. Write its XML through the entry writer");

            return writePage(page, xml, context);
        }, source);
    }

    Result<void> DictionaryBuild::writePage(const uint32_t page, std::string_view xml, SourceContext context)
    {
        const auto source = origin("writePage", "page " + std::to_string(page), context);
        return mutate([&]() -> Result<void>
        {
            auto& work = working();
            if (auto valid = work.entries.recordPage(page); !valid)
                return valid;

            auto analysis = content::analyzeArticle(page, xml);
            if (!analysis)
                return std::unexpected(analysis.error().describe("page " + std::to_string(page), xml));

            if (auto recorded = work.references.record(*analysis, source, "page.html", xml); !recorded)
                return recorded;

            work.references.define(analysis->anchors);
            work.pageOrigins.emplace(page, source);
            work.reservations.erase(page);

            if (!work.pages)
            {
                auto writer = ResourceStoreWriter::create(root_ / monokakido::paths::contents, metadata_.identifier,
                                                          {.encrypt = work.options.encryptContents});
                if (!writer)
                    return std::unexpected(writer.error());

                work.pages = std::move(*writer);
            }

            auto result = work.pages->add(page, {reinterpret_cast<const uint8_t*>(xml.data()), xml.size()});
            if (result)
                work.entries.written(page);

            return result;
        }, source);
    }

    Result<void> DictionaryBuild::addHeadline(HeadlineComponents headline, const HeadlineRole role,
                                              SourceContext context)
    {
        const auto source = origin("addHeadline", headline.entryId.toString(), context);
        return mutate([&]() -> Result<void>
        {
            if (auto valid = WriteValidation::entryId(headline.entryId); !valid)
                return valid;

            const auto origin = "headline " + headline.entryId.toString();
            const auto text = headline.full();
            auto analysis = content::analyzeHeadline(text);
            if (!analysis)
                return std::unexpected(analysis.error().describe(origin, unicode::toUtf8(text)));

            if (!analysis->references.empty())
            {
                const auto utf8 = unicode::toUtf8(text);
                if (auto recorded = working().references.record(*analysis, source, {}, utf8); !recorded)
                    return recorded;
            }

            working().references.requireTarget(headline.entryId, source);
            return working().indexes.addHeadline(std::move(headline), role);
        }, source);
    }

    Result<void> DictionaryBuild::addSearchKey(const SearchScope scope, std::string_view key, const EntryId target,
                                               const SearchKeyOptions options, SourceContext context)
    {
        const auto product = product_.lock();
        const auto definition = search::scopeDefinition(scope, product ? product->profile() : CreationProfile::General);
        const auto scopeName =
            definition ? std::string(definition->filename) : std::to_string(static_cast<unsigned>(scope));
        const auto source =
            origin("addSearchKey", scopeName + " key '" + std::string(key) + "' for " + target.toString(), context);
        return mutate([&]() -> Result<void>
        {
            auto result = working().indexes.addSearchKey(scope, key, target, options, source);
            if (result)
                working().references.requireTarget(target, source);

            return result;
        }, source);
    }

    Result<void> DictionaryBuild::writeAsset(std::string_view path, std::span<const uint8_t> bytes,
                                             SourceContext context)
    {
        const auto source = origin("writeAsset", std::string(path), context);
        return mutate([&]() -> Result<void>
        {
            if (auto valid = AssetWriter::validateAssetPath(path); !valid)
                return valid;

            if (path.ends_with(".css"))
            {
                const std::string_view css(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                auto analysis = content::analyzeCss(css);
                if (!analysis)
                    return std::unexpected(analysis.error().describe(path, css));

                analysis->diagnostics.clear();
                if (auto recorded = working().references.record(*analysis, source, path, css); !recorded)
                    return recorded;
            }

            auto written = working().assets.writeAsset(path, bytes);
            if (written && (htmlDocument(path) || path.ends_with(".css")))
                (*owner())->assetSource(metadata_.directory + "/" + std::string(path), context);

            return written;
        }, source);
    }

    Result<void> DictionaryBuild::writeMedia(std::string_view name, std::span<const uint8_t> bytes,
                                             const MediaKind kind, SourceContext context)
    {
        const auto source = origin("writeMedia", std::string(name), context);
        return mutate([&]() -> Result<void>
        {
            return working().assets.writeMedia(name, bytes, kind);
        }, source);
    }

    Result<GaijiResource> DictionaryBuild::addGaiji(std::string_view filename, std::span<const uint8_t> bytes,
                                                    SourceContext context)
    {
        const auto source = origin("addGaiji", std::string(filename), context);
        return mutate([&]() -> Result<GaijiResource>
        {
            if (filename.contains('/'))
                return std::unexpected("Gaiji filename must be a single path component");

            const auto path = std::string(monokakido::headline::gaijiDirectory) + std::string(filename);
            if (auto valid = AssetWriter::validateGaiji(path, bytes); !valid)
                return std::unexpected(valid.error());

            if (auto written = writeAsset(path, bytes, context); !written)
                return std::unexpected(written.error());

            return GaijiResource{path};
        }, source);
    }

    Result<void> DictionaryBuild::setPresentation(DictionaryPresentation presentation, SourceContext context)
    {
        const auto source = origin("setPresentation", "presentation", context);
        return mutate([&]() -> Result<void>
        {
            if (auto valid = metadata_preparation::validatePresentation(presentation); !valid)
                return valid;

            metadata_.presentation = std::move(presentation);
            working().presentationOrigin = source;
            return {};
        }, source);
    }

    Result<void> DictionaryBuild::setEntryOrder(std::span<const uint32_t> pages, SourceContext context)
    {
        const auto source = origin("setEntryOrder", "entry order", context);
        return mutate([&]() -> Result<void>
        {
            working().orderOrigin = source;
            return working().entries.setOrder(pages);
        }, source);
    }

    void DictionaryBuild::collectIssues(IssueCollector& issues) const
    {
        productReferences().collectIssues(issues, metadata_);
        if (finished())
            return;

        const auto& work = working();
        const auto add =
            [&](std::string code, std::string subject, std::string message, const AuthoringOrigin* source = nullptr)
        {
            DependencyOrigins origins;
            origins.add(source && !source->operation.empty()
                            ? *source
                            : origin("preflight", subject, metadataContext_));
            issues.add(std::move(code), metadata_, std::move(subject), std::move(message), origins);
        };

        if (work.entries.pages().empty())
            add("missing-pages", "", "Dictionary has no pages");

        if (!work.indexes.primaryCount())
            add("missing-headlines", "", "Dictionary has no primary headlines");

        if (!work.indexes.hasHeadwords())
            add("missing-headwords", "", "Dictionary has no headword search keys");

        for (const auto& [page, source] : work.reservations)
            add("unwritten-entry", std::to_string(page), "Reserved entry has no XML: page " + std::to_string(page),
                &source);

        work.references.validateTargets(issues, metadata_);
        for (auto page : work.entries.pages())
            if (!work.indexes.hasPrimary({page, 0}))
                add("missing-page-headline", std::to_string(page),
                    "Page " + std::to_string(page) + " requires a primary headline", &work.pageOrigins.at(page));

        for (const auto& [path, origins] : work.references.requiredAssets())
            if (!work.assets.contains(path))
                issues.add("missing-resource", metadata_, path, "Missing resource " + path, origins);

        for (const auto& [name, origins] : work.references.requiredGaiji())
            if (!work.assets.contains("gaiji/" + name + ".png") && !work.assets.contains("gaiji/" + name + ".svg"))
                issues.add("missing-gaiji", metadata_, name, "Missing gaiji " + name, origins);

        const auto& stylesheet = metadata_.presentation.fullRubyStylesheet;
        if (stylesheet && !work.assets.containsLoose(*stylesheet))
            add("missing-full-ruby-stylesheet", *stylesheet, "Missing full-ruby stylesheet: " + *stylesheet,
                &work.presentationOrigin);

        if (auto valid = work.entries.validateOrder(); !valid)
            add("invalid-entry-order", "entry order", valid.error(), &work.orderOrigin);

        work.indexes.validatePassages(work.references, issues, metadata_);
    }

    Result<ValidationReport> DictionaryBuild::preflight(ValidationOptions options) const
    {
        if (auto product = owner(); !product)
            return std::unexpected(product.error());

        IssueCollector issues(options);
        collectIssues(issues);
        return std::move(issues).take();
    }

    Result<void> DictionaryBuild::finish()
    {
        auto product = owner();
        if (!product)
            return std::unexpected(product.error());

        auto report = preflight({});
        if (!report)
            return std::unexpected(report.error());
        if (!report->valid())
            return std::unexpected(report->describe());

        auto result = [&]() -> Result<void>
        {
            auto& work = working();
            if (auto written = work.indexes.finish(); !written)
                return written;
            if (auto written = work.pages->finish(); !written)
                return written;
            if (auto written = work.assets.finish(); !written)
                return written;
            if (auto written = work.entries.writeOrder(root_); !written)
                return written;
            if (auto written = metadata_writer::writeContent(root_, metadata_, work.indexes.primaryCount(),
                                                             work.assets.audioCount());
                !written)
                return written;

            Finished completed{
                work.indexes.scopes(), std::move(work.references).takeTargets(),
                std::move(work.appendices), std::move(work.references).takeProductReferences(),
                work.assets.resourcePaths()
            };
            // Product-level links need only the sealed target catalogue, not the parsing and writing state.
            state_.emplace<Finished>(std::move(completed));
            return {};
        }();

        if (!result)
            return (*product)->fail(result.error());

        return result;
    }

    bool DictionaryBuild::hasResource(std::string_view path) const
    {
        if (!finished())
            return working().assets.contains(path);
        return std::get<Finished>(state_).resources.contains(path);
    }

    Result<void> DictionaryBuild::validateTarget(const EntryId target) const
    {
        if (auto valid = WriteValidation::entryId(target); !valid)
            return valid;

        const bool exists = finished()
                                ? std::ranges::binary_search(std::get<Finished>(state_).targets, target)
                                : working().references.contains(target);
        if (!exists)
            return std::unexpected("Missing entry target " + target.toString());

        return {};
    }

    Result<void> DictionaryBuild::addFont(std::string_view filename, std::span<const uint8_t> bytes,
                                          SourceContext context)
    {
        const auto source = origin("addFont", std::string(filename), context);
        return mutate([&]() -> Result<void>
        {
            if (auto valid = WriteValidation::relativePath(filename); !valid)
                return valid;

            if (filename.contains('/'))
                return std::unexpected("Font filename must be a single path component");

            const auto extension = WriteValidation::fromUtf8(filename).extension().string();
            if (!FontFormat::supportsExtension(extension))
                return std::unexpected("Font filename must use .ttf, .otf or .ttc");

            const auto format = FontFormat::detect(bytes);
            if (!format || *format != extension)
                return std::unexpected("Font signature must match its filename extension");

            auto product = product_.lock();
            if (product->hasFont(filename))
                return std::unexpected("Font filename must be unique within the product");

            DirectoryOutput fonts(root_ / monokakido::paths::fonts, false, false);
            if (auto ready = fonts.prepare(); !ready)
                return ready;

            auto written = fonts.writeFile(WriteValidation::fromUtf8(filename),
                                           [bytes](OutputFile& file) { return file.write(bytes); });
            if (!written)
                return std::unexpected(written.error());

            if (written->skipped)
                return std::unexpected("Duplicate font filename");

            working().assets.registerLoose(std::string(monokakido::paths::fonts) + "/" + std::string(filename));
            product->registerFont(std::string(filename));
            return {};
        }, source);
    }

    Result<AppendixDocument> DictionaryBuild::addAppendixDocument(std::string_view relative, std::string_view title,
                                                                  SourceContext context)
    {
        const auto source = origin("addAppendixDocument", std::string(title), context);
        return mutate([&]() -> Result<AppendixDocument>
        {
            if (auto valid = WriteValidation::relativePath(relative); !valid)
                return std::unexpected(valid.error());

            if (auto valid = WriteValidation::xmlText(title); !valid)
                return std::unexpected(valid.error());

            const auto path = relative.substr(0, relative.find('#'));
            const auto extension = WriteValidation::fromUtf8(path).extension();
            if (extension != ".html" && extension != ".pdf")
                return std::unexpected("Document appendices must be HTML or PDF assets");

            working().references.requireAsset(std::string(path), source);
            const auto hash = relative.find('#');
            AppendixDocument document{
                metadata_.directory + "/" + std::string(path),
                hash == std::string_view::npos ? std::nullopt : std::optional(std::string(relative.substr(hash + 1)))
            };
            working().appendices.push_back({.name = std::string(title), .target = document});
            return document;
        }, source);
    }

    Result<AppendixList> DictionaryBuild::addAppendixList(std::string_view name, std::string_view title,
                                                          std::span<const AppendixEntry> entries,
                                                          const AppendixListWriteOptions options, SourceContext context)
    {
        const auto source = origin("addAppendixList", std::string(title), context);
        return mutate([&]() -> Result<AppendixList>
        {
            if (auto valid = WriteValidation::relativePath(name); !valid)
                return std::unexpected(valid.error());

            if (auto valid = WriteValidation::xmlText(title); !valid)
                return std::unexpected(valid.error());

            if (auto valid = WriteValidation::xmlText(name); !valid)
                return std::unexpected(valid.error());

            if (name.contains('/'))
                return std::unexpected("Appendix name must be a single path component");

            const auto relative = "appendix/" + std::string(name) + (name.ends_with(".entries") ? "" : ".entries");
            auto writer = AppendixEntryListWriter::create(root_ / WriteValidation::fromUtf8(relative), options);
            if (!writer)
                return std::unexpected(writer.error());

            size_t row = 0;
            for (const auto& entry : entries)
            {
                if (entry.entryId.pageId || entry.entryId.itemId)
                    if (auto valid = WriteValidation::entryId(entry.entryId); !valid)
                        return std::unexpected(valid.error());

                auto rowOrigin = source;
                rowOrigin.subject += " row " + std::to_string(++row);
                auto target = entry;
                if (!target.dictionaryId && target.entryId.pageId)
                    target.dictionaryId = *metadata_.dictionaryId;

                if (entry.entryId.pageId)
                {
                    if (target.dictionaryId == *metadata_.dictionaryId)
                        working().references.requireTarget(entry.entryId, rowOrigin);
                    else
                    {
                        auto product = owner();
                        if (!product)
                            return std::unexpected(product.error());

                        (*product)->referenceAppendix({entry.entryId, target.dictionaryId}, metadata_, rowOrigin);
                    }
                }
                if (auto result = writer->add(std::move(target)); !result)
                    return std::unexpected(result.error());
            }
            if (auto result = writer->finish(); !result)
                return std::unexpected(result.error());

            AppendixList list{metadata_.directory + "/" + relative};
            working().appendices.push_back({.name = std::string(title), .target = list});
            working().assets.registerLoose(relative);
            return list;
        }, source);
    }
}
