//
// kiwakiwaaにより 2026/10/24 に作成されました。
//

#pragma once

#include "MKD/dictionary/dictionary_writer.hpp"
#include "MKD/resource/resource_store_writer.hpp"
#include "entry_allocator.hpp"
#include "reference_index.hpp"
#include "search_index_builder.hpp"
#include "asset_writer.hpp"
#include <type_traits>
#include <utility>

namespace MKD::detail
{
    class ProductBuild;

    class DictionaryBuild
    {
    public:
        DictionaryBuild(std::shared_ptr<ProductBuild> product, DictionaryContent metadata,
                        DictionaryWriteOptions options, SourceContext context);
        Result<uint32_t> reserveEntry(std::optional<uint32_t> page, SourceContext context = {});
        Result<EntryId> addTarget(uint32_t page, EntryTargetKind kind, SourceContext context = {});
        Result<void> addPage(uint32_t page, std::string_view xml, SourceContext context = {});
        Result<void> writePage(uint32_t page, std::string_view xml, SourceContext context = {});
        Result<void> addHeadline(HeadlineComponents headline, HeadlineRole role, SourceContext context = {});
        Result<void> addSearchKey(SearchScope scope, std::string_view key, EntryId target, SearchKeyOptions options,
                                  SourceContext context = {});
        Result<GaijiResource> addGaiji(std::string_view filename, std::span<const uint8_t> bytes,
                                       SourceContext context = {});
        Result<void> setPresentation(DictionaryPresentation presentation, SourceContext context = {});
        Result<void> writeAsset(std::string_view path, std::span<const uint8_t> bytes, SourceContext context = {});
        Result<void> writeMedia(std::string_view name, std::span<const uint8_t> bytes, MediaKind kind,
                                SourceContext context = {});
        Result<void> addFont(std::string_view filename, std::span<const uint8_t> bytes, SourceContext context = {});
        Result<AppendixDocument> addAppendixDocument(std::string_view relative, std::string_view title,
                                                     SourceContext context = {});
        Result<AppendixList> addAppendixList(std::string_view name, std::string_view title,
                                             std::span<const AppendixEntry> entries, AppendixListWriteOptions options,
                                             SourceContext context = {});
        Result<void> setEntryOrder(std::span<const uint32_t> pages, SourceContext context = {});
        Result<ValidationReport> preflight(ValidationOptions options) const;
        void collectIssues(IssueCollector& issues) const;
        Result<void> finish();
        Result<void> validateTarget(EntryId target) const;

        const ProductReferences& productReferences() const
        {
            return finished() ? std::get<Finished>(state_).references : working().references.productReferences();
        }

        [[nodiscard]] bool hasResource(std::string_view path) const;
        [[nodiscard]] const DictionaryContent& metadata() const { return metadata_; }
        [[nodiscard]] const std::filesystem::path& root() const { return root_; }
        [[nodiscard]] bool finished() const { return std::holds_alternative<Finished>(state_); }

        [[nodiscard]] std::set<SearchScope> scopes() const
        {
            return finished() ? std::get<Finished>(state_).scopes : working().indexes.scopes();
        }

        [[nodiscard]] const std::vector<AppendixItem>& appendices() const
        {
            return finished() ? std::get<Finished>(state_).appendices : working().appendices;
        }

    private:
        [[nodiscard]] Result<std::shared_ptr<ProductBuild>> owner() const;
        void fail(std::string error) const;

        template <typename Operation>
        auto mutate(Operation&& operation, const AuthoringOrigin& source) -> std::invoke_result_t<Operation>
        {
            using Return = std::invoke_result_t<Operation>;
            if (auto product = owner(); !product)
                return Return(std::unexpected(product.error()));

            auto result = std::forward<Operation>(operation)();
            if (!result)
            {
                result = std::unexpected(metadata_.identifier + ": " + source.describe() + ": " + result.error());
                fail(result.error());
            }

            return result;
        }

        struct Working
        {
            Working(const std::filesystem::path& root, CreationProfile profile, DictionaryWriteOptions options)
                : entries(profile), indexes(root, profile), assets(root), options(options)
            {
            }

            EntryAllocator entries;
            SearchIndexBuilder indexes;
            AssetWriter assets;
            ReferenceIndex references;
            DictionaryWriteOptions options;
            std::optional<ResourceStoreWriter> pages;
            std::vector<AppendixItem> appendices;
            std::map<uint32_t, AuthoringOrigin> pageOrigins;
            std::map<uint32_t, AuthoringOrigin> reservations;
            AuthoringOrigin orderOrigin;
            AuthoringOrigin presentationOrigin;
        };

        struct Finished
        {
            std::set<SearchScope> scopes;
            std::vector<EntryId> targets;
            std::vector<AppendixItem> appendices;
            ProductReferences references;
            std::set<std::string, std::less<>> resources;
        };

        std::weak_ptr<ProductBuild> product_;
        DictionaryContent metadata_;
        std::filesystem::path root_;
        SourceContext metadataContext_;
        Working& working() { return std::get<Working>(state_); }
        const Working& working() const { return std::get<Working>(state_); }
        std::variant<Working, Finished> state_;
    };
}
