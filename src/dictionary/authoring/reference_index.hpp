//
// kiwakiwaaにより 2026/10/24 に作成されました。
//

#pragma once

#include "MKD/dictionary/content_analysis.hpp"
#include "MKD/result.hpp"
#include "authoring_diagnostics.hpp"

#include <map>
#include <set>
#include <unordered_set>

namespace MKD::detail
{
    class DocumentChecks;

    /** Product-level dependencies retained after the dictionary's writing state is released. */
    struct ProductReferences
    {
        std::map<std::pair<uint16_t, EntryId>, DependencyOrigins> entries;
        std::map<std::pair<std::string, std::optional<std::string>>, DependencyOrigins> documents;
        std::map<std::pair<content::DiagnosticKind, std::string>, DependencyOrigins> diagnostics;
        void collect(DocumentChecks& checks, const DictionaryContent& dictionary) const;
        void collectIssues(IssueCollector& issues, const DictionaryContent& dictionary) const;
    };

    class ReferenceIndex
    {
    public:
        Result<void> record(const content::Analysis& analysis, AuthoringOrigin origin, std::string_view sourcePath = {},
                            std::string_view input = {});
        void define(const std::set<EntryId>& targets);
        void requireTarget(EntryId target, AuthoringOrigin origin);
        void requireAsset(std::string path, AuthoringOrigin origin);

        [[nodiscard]] bool contains(const EntryId target) const
        {
            return anchors_.contains(target);
        }

        [[nodiscard]] bool references(const EntryId target) const
        {
            return targets_.contains(target);
        }

        void validateTargets(IssueCollector& issues, const DictionaryContent& dictionary) const;

        [[nodiscard]] const auto& requiredAssets() const
        {
            return assets_;
        }

        [[nodiscard]] const auto& requiredGaiji() const
        {
            return gaiji_;
        }

        std::vector<EntryId> takeTargets() &&;

        const ProductReferences& productReferences() const
        {
            return product_;
        }

        ProductReferences takeProductReferences() &&
        {
            return std::move(product_);
        }

    private:
        Result<void> record(const content::EntryReference& target, const AuthoringOrigin& origin,
                            std::string_view sourcePath);
        Result<void> record(const content::DocumentReference& target, const AuthoringOrigin& origin,
                            std::string_view sourcePath);
        Result<void> record(const content::AssetReference& target, const AuthoringOrigin& origin,
                            std::string_view sourcePath);
        Result<void> record(const content::GaijiReference& target, const AuthoringOrigin& origin,
                            std::string_view sourcePath);
        Result<void> record(const content::ExternalReference& target, const AuthoringOrigin& origin,
                            std::string_view sourcePath);

        ProductReferences product_;
        std::unordered_set<EntryId> anchors_;
        std::map<EntryId, DependencyOrigins> targets_;
        std::map<std::string, DependencyOrigins, std::less<>> assets_;
        std::map<std::string, DependencyOrigins, std::less<>> gaiji_;
    };
}
