#include "MKD/dictionary/content_analysis.hpp"
#include "html_lexer.hpp"
#include "analysis_support.hpp"
#include "io/write_validation.hpp"
#include <algorithm>

namespace MKD::content
{
    class DocumentReferenceCollector
    {
    public:
        AnalysisResult<DocumentAnalysis> analyze(std::string_view html);

    private:
        void element(const detail::html::Token& token);
        void attribute(const detail::html::Token& token);
        void stylesheet(std::string_view css, SourceRange source, CssInput input);
        DocumentAnalysis result_;
        detail::html::Token element_;
        std::vector<std::string_view> attributes_;
        std::set<std::string> elementIdentifiers_;
        bool hasBase_ = false;
    };

    void DocumentReferenceCollector::element(const detail::html::Token& token)
    {
        element_ = token;
        attributes_.clear();
        elementIdentifiers_.clear();
        if (token.named("base"))
        {
            hasBase_ = true;
            result_.diagnostics.push_back(
                {
                    DiagnosticKind::UnsupportedDocumentFeature, token.source,
                    "HTML base URLs are not resolved. Relative references in this document are unchecked."
                });
        }
        if (token.named("script"))
            result_.diagnostics.push_back({
                DiagnosticKind::UnsupportedDocumentFeature, token.source,
                "Script-generated references are not checked."
            });
    }

    void DocumentReferenceCollector::stylesheet(std::string_view css, SourceRange source, CssInput input)
    {
        auto analysis = analyzeCss(css, input);
        if (!analysis)
        {
            result_.diagnostics.push_back({
                DiagnosticKind::UnsupportedDocumentFeature, source,
                "Cannot analyse document CSS: " + analysis.error().message
            });
            return;
        }

        for (auto& reference : analysis->references)
        {
            reference.source = source;
            result_.references.push_back(std::move(reference));
        }
        // Article-renderer CSS limitations do not apply to loose HTML document views.
    }

    void DocumentReferenceCollector::attribute(const detail::html::Token& token)
    {
        if (std::ranges::any_of(attributes_, [&](std::string_view name) { return token.named(name); }))
            return;

        attributes_.push_back(token.name);
        const bool identifier = token.named("id") || (element_.named("a") && token.named("name"));
        const bool style = token.named("style");
        const bool link = (element_.named("a") || element_.named("area")) && token.named("href");
        const bool resource = token.named("src") || token.named("poster") ||
            (element_.named("object") && token.named("data")) ||
            (element_.named("link") && token.named("href"));

        if (token.named("srcset") || token.name.starts_with("on"))
            result_.diagnostics.push_back(
                {
                    DiagnosticKind::UnsupportedDocumentFeature, token.source,
                    "Dynamic or responsive resource attribute is not checked: " + std::string(token.name)
                });

        if (!identifier && !style && !link && !resource)
            return;

        auto decoded = token.decodedValue();
        if (!decoded)
        {
            if (identifier)
                result_.identifiersComplete = false;

            result_.diagnostics.push_back({DiagnosticKind::UnsupportedDocumentFeature, token.source, decoded.error()});
            return;
        }

        if (identifier)
        {
            if (!decoded->empty() && elementIdentifiers_.insert(*decoded).second &&
                !result_.identifiers.insert(*decoded).second)
                result_.diagnostics.push_back({
                    DiagnosticKind::DuplicateIdentifier, token.source,
                    "Duplicate document fragment identifier: " + *decoded
                });
        }
        else if (style)
            stylesheet(*decoded, token.source, CssInput::Declarations);
        else if (link)
            result_.references.push_back(detail::linkReference(*decoded, token.source));
        else if (auto reference = detail::resourceReference(*decoded, ResourceSyntax::XmlAttribute, token.source))
            result_.references.push_back(std::move(*reference));
    }

    AnalysisResult<DocumentAnalysis> DocumentReferenceCollector::analyze(std::string_view html)
    {
        detail::html::Lexer lexer(html);
        for (;;)
        {
            auto token = lexer.next();
            if (!token)
            {
                result_.identifiersComplete = false;
                result_.diagnostics.push_back(
                    {DiagnosticKind::UnsupportedDocumentFeature, token.error().source, token.error().message});
                break;
            }

            if (token->kind == detail::html::TokenKind::End)
                break;

            switch (token->kind)
            {
            case detail::html::TokenKind::StartElement:
                element(*token);
                break;
            case detail::html::TokenKind::Attribute:
                attribute(*token);
                break;
            case detail::html::TokenKind::Text:
                if (token->named("style"))
                    stylesheet(token->value, token->source, CssInput::Stylesheet);
                break;
            default:
                break;
            }
        }

        if (hasBase_)
            std::erase_if(result_.references,
                          [](const Reference& reference)
                          {
                              return !std::holds_alternative<ExternalReference>(reference.target) &&
                                  !std::holds_alternative<EntryReference>(reference.target);
                          });

        return std::move(result_);
    }

    AnalysisResult<DocumentAnalysis> analyzeDocument(std::string_view html)
    {
        if (auto valid = detail::WriteValidation::text(html, true); !valid)
            return std::unexpected(SourceError{valid.error(), {}});
        return DocumentReferenceCollector{}.analyze(html);
    }
}
