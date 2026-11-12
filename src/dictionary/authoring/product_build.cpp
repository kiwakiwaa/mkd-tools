//
// kiwakiwaaにより 2026/10/24 に作成されました。
//

#include "product_build.hpp"
#include "dictionary/content/document_checks.hpp"
#include "dictionary_build.hpp"
#include "product_information.hpp"
#include "product_icon.hpp"
#include "dictionary/authoring/metadata_preparation.hpp"
#include "dictionary/package/metadata_writer.hpp"
#include "dictionary/package/package_format.hpp"
#include "dictionary/search/search_definitions.hpp"
#include "dictionary/authoring/navigation_writer.hpp"
#include "dictionary/navigation/index_format.hpp"
#include "dictionary/authoring/appendix_writer.hpp"
#include "io/write_validation.hpp"
#include "output/directory_output.hpp"
#include <algorithm>
#include <ranges>

namespace MKD::detail
{
    ProductBuild::ProductBuild(std::unique_ptr<BuildDirectory> output, DictionaryMetadata metadata, std::string name,
                               const CreationProfile profile)
        : output_(std::move(output)), metadata_(std::move(metadata)), name_(std::move(name)), profile_(profile)
    {
    }

    const DictionaryContent& ProductBuild::firstContent() const
    {
        return dictionaries_.front()->metadata();
    }

    bool ProductBuild::hasFont(std::string_view filename) const
    {
        return std::ranges::find(metadata_.fonts, filename) != metadata_.fonts.end();
    }

    Result<void> ProductBuild::validateTarget(const AppendixEntryLink& target) const
    {
        const auto dictionary = std::ranges::find_if(dictionaries_, [&](const auto& item)
        {
            return item->metadata().dictionaryId == target.dictionaryId;
        });

        if (dictionary == dictionaries_.end())
            return std::unexpected("Entry targets an unknown dictionary");

        return (*dictionary)->validateTarget(target.entryId);
    }

    void ProductBuild::referenceAppendix(AppendixEntryLink target, const DictionaryContent& dictionary,
                                         AuthoringOrigin source)
    {
        auto& reference =
            appendixReferences_
            .try_emplace(std::tuple{*dictionary.dictionaryId, target.dictionaryId, target.entryId},
                         AppendixReference{target, dictionary.dictionaryId, dictionary.identifier, {}})
            .first->second;
        reference.origins.add(std::move(source));
    }

    Result<void> ProductBuild::setAppendices(AppendixMenu value, SourceContext context)
    {
        if (auto state = output_->check(); !state)
            return state;

        appendices_ = std::move(value);
        appendixContext_ = std::move(context);
        return {};
    }

    Result<void> ProductBuild::setNavigation(NavigationIndex value, SourceContext context)
    {
        if (auto state = output_->check(); !state)
            return state;

        navigation_ = std::move(value);
        navigationContext_ = std::move(context);
        return {};
    }

    Result<void> ProductBuild::setInformation(ProductInformation value, SourceContext context)
    {
        const auto reject = [&](std::string message)
        {
            return output_->fail(origin("setInformation", "information", context).describe() + ": " + message);
        };
        if (auto state = output_->check(); !state)
            return state;

        if (auto valid = ProductInformationWriter::validate(value); !valid)
            return reject(valid.error());

        information_ = std::move(value);
        informationContext_ = std::move(context);
        return {};
    }

    Result<void> ProductBuild::setIcon(std::span<const uint8_t> bytes, SourceContext context)
    {
        const auto reject = [&](std::string message)
        {
            return output_->fail(origin("setIcon", "icon", context).describe() + ": " + message);
        };
        if (auto state = output_->check(); !state)
            return state;

        if (auto written = ProductIcon::write(*this, bytes, context); !written)
            return reject(written.error());

        return {};
    }

    Result<std::shared_ptr<ProductBuild>> ProductBuild::create(const std::filesystem::path& destination,
                                                               DictionaryMetadata metadata, CreationProfile profile)
    {
        auto prepared = metadata_preparation::prepareProduct(std::move(metadata), destination, profile);
        if (!prepared)
            return std::unexpected(prepared.error());

        metadata = std::move(*prepared);

        auto output = BuildDirectory::create(destination);
        if (!output)
            return std::unexpected(output.error());

        std::error_code error;
        std::filesystem::create_directory((*output)->path() / monokakido::paths::productContents, error);
        if (error)
            return std::unexpected(error.message());

        const auto name = destination.stem().generic_u8string();
        auto product = std::make_shared<ProductBuild>(
            std::move(*output), std::move(metadata),
            std::string(reinterpret_cast<const char*>(name.data()), name.size()), profile);

        return product;
    }

    Result<std::shared_ptr<DictionaryBuild>> ProductBuild::addDictionary(DictionaryContent content,
                                                                         DictionaryWriteOptions options,
                                                                         SourceContext context)
    {
        const auto reject = [&](std::string message)
        {
            return output_->fail(origin("addDictionary", "dictionary", context).describe() + ": " + message);
        };
        if (auto state = output_->check(); !state)
            return std::unexpected(state.error());

        auto prepared = metadata_preparation::prepareContent(std::move(content));
        if (!prepared)
        {
            const auto rejected = reject(prepared.error());
            return std::unexpected(rejected.error());
        }

        content = std::move(*prepared);
        for (const auto& existing : dictionaries_)
        {
            if (existing->metadata().identifier == content.identifier ||
                existing->metadata().directory == content.directory ||
                existing->metadata().dictionaryId == content.dictionaryId)
            {
                const auto rejected = reject("Duplicate content identifier, directory or numeric dictionary ID");
                return std::unexpected(rejected.error());
            }
        }

        auto dictionary = std::make_shared<DictionaryBuild>(shared_from_this(), std::move(content), options, context);
        std::error_code error;
        if (!std::filesystem::create_directory(dictionary->root(), error))
        {
            const auto message =
                "Cannot create content directory: " + (error ? error.message() : "path already exists");
            const auto rejected = reject(message);
            return std::unexpected(rejected.error());
        }

        dictionaries_.push_back(dictionary);
        return dictionary;
    }

    Result<void> ProductBuild::validateAssetPath(std::string_view relative) const
    {
        if (auto valid = WriteValidation::relativePath(relative); !valid)
            return std::unexpected(valid.error());

        if (relative == name_ + ".json" || relative == monokakido::paths::appendixInfo ||
            relative == monokakido::paths::moreInfo)
            return std::unexpected("Asset path is reserved for generated product metadata");

        const auto path = WriteValidation::fromUtf8(relative);
        if (*path.begin() == monokakido::paths::appendixMenus)
            return std::unexpected("Asset path is reserved for generated appendix menus");

        for (const auto& dictionary : dictionaries_)
            if (*path.begin() == WriteValidation::fromUtf8(dictionary->metadata().directory))
                return std::unexpected("Use the dictionary writer for content assets");

        return {};
    }

    Result<void> ProductBuild::writeAsset(std::string_view relative, std::span<const uint8_t> bytes,
                                          SourceContext context) const
    {
        const auto reject = [&](std::string message)
        {
            return output_->fail(origin("writeAsset", std::string(relative), context).describe() + ": " + message);
        };
        if (auto state = output_->check(); !state)
            return state;

        if (auto valid = validateAssetPath(relative); !valid)
            return reject(valid.error());
        const auto path = WriteValidation::fromUtf8(relative);

        DirectoryOutput directory(output_->path() / monokakido::paths::productContents, false, false);
        if (auto prepared = directory.prepare(); !prepared)
            return reject(prepared.error());

        auto written = directory.writeFile(path, [bytes](OutputFile& file) { return file.write(bytes); });
        if (!written)
            return reject(written.error());

        if (written->skipped)
            return reject("Duplicate product asset");

        if (htmlDocument(relative) || relative.ends_with(".css"))
            assetSources_.insert_or_assign(std::string(relative), context);

        return {};
    }

    AppendixMenu ProductBuild::appendixMenu() const
    {
        if (appendices_)
            return *appendices_;

        AppendixMenu menu;
        for (const auto& dictionary : dictionaries_)
        {
            if (dictionary->appendices().empty())
                continue;
            AppendixSection section{
                dictionary->metadata().title
                    ? dictionary->metadata().title->resolve()
                    : dictionary->metadata().directory,
                dictionary->appendices()
            };
            menu.sections.push_back(std::move(section));
        }
        return menu;
    }

    void ProductBuild::assetSource(std::string path, SourceContext source) const
    {
        assetSources_.insert_or_assign(std::move(path), std::move(source));
    }

    void ProductBuild::collectIssues(IssueCollector& issues) const
    {
        if (dictionaries_.empty())
        {
            issues.add("missing-dictionaries", "product", "Product has no dictionaries",
                       origin("preflight", "product"));
        }

        for (const auto& dictionary : dictionaries_)
        {
            dictionary->collectIssues(issues);
            if (!dictionary->finished())
            {
                issues.pending(dictionary->metadata().identifier);
            }
        }

        for (const auto& reference : appendixReferences_ | std::views::values)
        {
            if (auto valid = validateTarget(reference.target); !valid)
            {
                issues.add({
                    .code = "invalid-appendix-target",
                    .dictionaryId = reference.dictionaryId,
                    .dictionary = reference.dictionary,
                    .subject = std::to_string(reference.target.dictionaryId) + "/" +
                    reference.target.entryId.toString(),
                    .message = valid.error(),
                    .occurrences = reference.origins.occurrences,
                    .origins = reference.origins.samples
                });
            }
        }
    }

    DictionaryMetadata ProductBuild::prepareMetadata() const
    {
        auto metadata = metadata_;
        std::set<SearchScope> scopes;
        for (const auto& dictionary : dictionaries_)
        {
            metadata.contents.push_back(dictionary->metadata());
            const auto dictionaryScopes = dictionary->scopes();
            scopes.insert(dictionaryScopes.begin(), dictionaryScopes.end());
        }

        if (profile_ == CreationProfile::Kanwa)
        {
            for (const auto& scope : monokakido::search::kanwaScopes)
            {
                if (scopes.contains(scope.scope))
                {
                    metadata.searchScopeNames.emplace_back(scope.name);
                }
            }
        }
        else
        {
            for (auto scope : scopes)
            {
                metadata.searchScopeNames.emplace_back(search::scopeDefinition(scope, profile_)->name);
            }
        }

        std::ranges::sort(metadata.fonts);
        return metadata;
    }

    Result<void> ProductBuild::checkResource(std::string_view path, const PreparedProductFiles& files) const
    {
        for (const auto& dictionary : dictionaries_)
        {
            const auto prefix = dictionary->metadata().directory + "/";
            if (path.starts_with(prefix) && dictionary->hasResource(path.substr(prefix.size())))
            {
                return {};
            }
        }

        if (files.contains(path))
        {
            return {};
        }

        return std::unexpected("Missing local resource: " + std::string(path));
    }

    void ProductBuild::prepareFiles(DictionaryMetadata& metadata, PreparedProductFiles& files, IssueCollector& issues,
                                    DocumentChecks& documents) const
    {
        ProductInformationWriter::prepare(*this, metadata, files, issues, documents, informationContext_);
        const auto menu = appendixMenu();
        if (!menu.sections.empty() || appendices_)
        {
            AppendixWriter::prepare(*this, menu, files, issues, documents, appendixContext_);
        }

        if (navigation_ && !dictionaries_.empty())
        {
            NavigationWriter::prepare(*this, *navigation_, menu, files, issues, documents, navigationContext_);
            metadata.appendixViewController = profile_ == CreationProfile::Kanwa
                                                  ? monokakido::navigation::kanwaController
                                                  : monokakido::navigation::generalController;
            metadata.appendixFirst = true;
        }
    }

    Result<ProductBuild::Prepared> ProductBuild::prepare(ValidationOptions options) const
    {
        if (auto state = check(); !state)
        {
            return std::unexpected(state.error());
        }

        IssueCollector issues(options);
        collectIssues(issues);
        auto metadata = prepareMetadata();
        PreparedProductFiles files(path() / monokakido::paths::productContents);
        DocumentChecks documents(issues,
                                 [&](std::string_view path) { return files.read(path); },
                                 [&](std::string_view path) { return checkResource(path, files); },
                                 [&](const AppendixEntryLink& target) { return validateTarget(target); },
                                 metadata.contents, assetSources_);

        prepareFiles(metadata, files, issues, documents);

        for (const auto& dictionary : dictionaries_)
        {
            dictionary->productReferences().collect(documents, dictionary->metadata());
        }
        if (options.scanAllDocuments)
        {
            documents.scan(path() / monokakido::paths::productContents);
        }

        documents.run();
        return Prepared{std::move(issues).take(), std::move(files), std::move(metadata)};
    }

    Result<ValidationReport> ProductBuild::preflight(const ValidationOptions options) const
    {
        return prepare(options).transform([](Prepared&& prepared) { return std::move(prepared.report); });
    }

    Result<void> ProductBuild::finish()
    {
        auto prepared = prepare({});
        if (!prepared)
            return std::unexpected(prepared.error());
        if (!prepared->report.ready())
            return std::unexpected(prepared->report.describe());

        // After emission begins, an error may leave partial product files in staging.
        metadata_ = std::move(prepared->metadata);
        if (auto written = prepared->files.emit(); !written)
            return fail(written.error());
        if (auto written = metadata_writer::writeProduct(path(), name_, metadata_); !written)
            return fail(written.error());

        return output_->publish();
    }
}
