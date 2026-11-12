//
// kiwakiwaaにより 2026/10/24 に作成されました。
//

#pragma once

#include "MKD/resource/entry_id.hpp"
#include "MKD/result.hpp"
#include <cstddef>
#include <set>
#include <optional>
#include <string_view>
#include <string>
#include <variant>
#include <vector>

namespace MKD::content
{
    /** UTF-8 byte range. A zero length identifies a position or containing element.
     * Headline ranges refer to the UTF-8 encoding of the supplied UTF-16 text. */
    struct SourceRange
    {
        size_t offset = 0;
        size_t length = 0;
    };

    struct SourceError
    {
        std::string message;
        std::optional<SourceRange> source;
        [[nodiscard]] std::string describe(std::string_view origin = {}, std::string_view input = {}) const;
    };

    template <typename T>
    using AnalysisResult = std::expected<T, SourceError>;

    enum class ResourceSyntax
    {
        Path,
        XmlAttribute,
        CssUrl
    };

    struct AssetReference
    {
        std::string path;
        ResourceSyntax syntax = ResourceSyntax::Path;
    };

    struct GaijiReference
    {
        std::string name;
    };
    struct ExternalReference
    {
        std::string url;
    };

    /** Missing or zero dictionary IDs use the current article dictionary, when one exists. */
    struct EntryReference
    {
        EntryId entryId;
        std::optional<uint16_t> dictionaryId;
    };
    /** An empty path refers to the current document. The fragment is stored without its leading hash. */
    struct DocumentReference
    {
        std::string path;
        std::optional<std::string> fragment;
        ResourceSyntax syntax = ResourceSyntax::XmlAttribute;
    };

    using ReferenceTarget = std::variant<EntryReference, DocumentReference, AssetReference, GaijiReference, ExternalReference>;

    struct Reference
    {
        ReferenceTarget target;
        std::optional<SourceRange> source;
    };

    using References = std::vector<Reference>;

    enum class DiagnosticKind
    {
        IgnoredAtRule,
        IgnoredStyleElement,
        NativeMarkup,
        UnsupportedDocumentFeature,
        DuplicateIdentifier
    };

    struct Diagnostic
    {
        DiagnosticKind kind;
        std::optional<SourceRange> source;
        std::string message;
    };

    /** Owns its references and diagnostics independently of the input buffer. */
    struct Analysis
    {
        References references;
        std::vector<Diagnostic> diagnostics;
    };

    struct ArticleAnalysis : Analysis
    {
        std::set<EntryId> anchors;
    };

    struct DocumentAnalysis : Analysis
    {
        std::set<std::string> identifiers;
        bool identifiersComplete = true;
    };

    enum class CssInput
    {
        Stylesheet,
        Declarations
    };

    /** Reports source references without resolving paths or loading resources.
     * CSS analysis scans dependencies and selected compatibility issues with the Dictionaries article renderer.
     * It does not validate the full CSS grammar. */
    /** Scans static HTML references without requiring XML well-formedness. */
    AnalysisResult<DocumentAnalysis> analyzeDocument(std::string_view html);
    AnalysisResult<ArticleAnalysis> analyzeArticle(uint32_t page, std::string_view xhtml);
    AnalysisResult<Analysis> analyzeCss(std::string_view css, CssInput input = CssInput::Stylesheet);
    AnalysisResult<Analysis> analyzeHeadline(std::u16string_view text);
}
