//
// kiwakiwaaにより 2026/10/24 に作成されました。
//

#include "reference_index.hpp"
#include "dictionary/content/document_checks.hpp"
#include "dictionary/content/analysis_support.hpp"
#include "dictionary/package/paths.hpp"
#include <algorithm>

namespace MKD::detail
{
    void ProductReferences::collect(DocumentChecks& checks, const DictionaryContent& dictionary) const
    {
        for (const auto& [target, origins] : entries)
        {
            const auto& [dictionaryId, entryId] = target;
            checks.entry({entryId, dictionaryId}, dictionary.dictionaryId, origins);
        }

        for (const auto& [target, origins] : documents)
        {
            const auto& [path, fragment] = target;
            checks.document(dictionary.directory + "/" + path, fragment, origins);
        }
    }

    void ProductReferences::collectIssues(IssueCollector& issues, const DictionaryContent& dictionary) const
    {
        for (const auto& [diagnostic, origins] : diagnostics)
            issues.add({
                .code = std::string(diagnosticCode(diagnostic.first)),
                .dictionaryId = dictionary.dictionaryId,
                .dictionary = dictionary.identifier,
                .subject = "article markup",
                .message = diagnostic.second,
                .occurrences = origins.occurrences,
                .origins = origins.samples,
                .severity = diagnosticSeverity(diagnostic.first)
            });
    }

    Result<void> ReferenceIndex::record(const content::Analysis& analysis, AuthoringOrigin origin,
                                        std::string_view sourcePath, std::string_view input)
    {
        const SourceMap map(input);
        for (const auto& diagnostic : analysis.diagnostics)
            product_.diagnostics[{diagnostic.kind, diagnostic.message}].add(
                locatedOrigin(origin, diagnostic.source, map));

        for (const auto& reference : analysis.references)
        {
            const auto source = locatedOrigin(origin, reference.source, map);
            auto result =
                std::visit([&](const auto& target) { return record(target, source, sourcePath); }, reference.target);
            if (!result)
                return std::unexpected(source.describe() + ": " + result.error());
        }
        return {};
    }

    Result<void> ReferenceIndex::record(const content::EntryReference& target, const AuthoringOrigin& origin,
                                        std::string_view)
    {
        if (const auto dictionary = target.dictionaryId.value_or(0))
            product_.entries[{dictionary, target.entryId}].add(origin);
        else
            targets_[target.entryId].add(origin);

        return {};
    }

    Result<void> ReferenceIndex::record(const content::DocumentReference& target, const AuthoringOrigin& origin,
                                        std::string_view sourcePath)
    {
        if (const auto entry = articleEntryReference(target))
            return record(*entry, origin, sourcePath);
        if (target.path.empty())
            return {};

        auto path = resolveAssetReference({target.path, target.syntax}, sourcePath);
        if (!path)
            return std::unexpected(path.error());
        if (*path)
        {
            assets_[**path].add(origin);
            product_.documents[{**path, target.fragment}].add(origin);
        }

        return {};
    }

    Result<void> ReferenceIndex::record(const content::AssetReference& target, const AuthoringOrigin& origin,
                                        std::string_view sourcePath)
    {
        auto path = resolveAssetReference(target, sourcePath);
        if (!path)
            return std::unexpected(path.error());
        if (*path)
            assets_[**path].add(origin);

        return {};
    }

    Result<void> ReferenceIndex::record(const content::GaijiReference& target, const AuthoringOrigin& origin,
                                        std::string_view)
    {
        gaiji_[target.name].add(origin);
        return {};
    }

    Result<void> ReferenceIndex::record(const content::ExternalReference&, const AuthoringOrigin&, std::string_view)
    {
        return {};
    }

    void ReferenceIndex::define(const std::set<EntryId>& targets)
    {
        anchors_.insert(targets.begin(), targets.end());
    }

    void ReferenceIndex::requireTarget(EntryId target, AuthoringOrigin origin)
    {
        targets_[target].add(std::move(origin));
    }

    void ReferenceIndex::requireAsset(std::string path, AuthoringOrigin origin)
    {
        assets_[std::move(path)].add(std::move(origin));
    }

    void ReferenceIndex::validateTargets(IssueCollector& issues, const DictionaryContent& dictionary) const
    {
        for (const auto& [target, origins] : targets_)
            if (!contains(target))
                issues.add("missing-entry-target", dictionary, target.toString(),
                           "Missing entry target " + target.toString(), origins);
    }

    std::vector<EntryId> ReferenceIndex::takeTargets() &&
    {
        std::vector<EntryId> targets(anchors_.begin(), anchors_.end());
        std::ranges::sort(targets);
        return targets;
    }
}
