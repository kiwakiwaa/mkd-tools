//
// kiwakiwaaにより 2026/10/24 に作成されました。
//

#include "MKD/dictionary/content_analysis.hpp"
#include "analysis_support.hpp"
#include "content_markup.hpp"
#include "io/write_validation.hpp"
#include <algorithm>
#include <pugixml.hpp>

namespace MKD::content
{
    using detail::WriteValidation;

    namespace
    {
        class ArticleWalker final : public pugi::xml_tree_walker
        {
        public:
            explicit ArticleWalker(uint32_t page) : page_(page) { result.anchors.insert({page, 0}); }
            ArticleAnalysis result;
            std::optional<SourceError> error;

            AnalysisResult<void> validateFragments() const
            {
                for (const auto& reference : result.references)
                {
                    const auto document = std::get_if<DocumentReference>(&reference.target);
                    if (!document || !document->path.empty() || !document->fragment || document->fragment->empty() ||
                        document->syntax != ResourceSyntax::XmlAttribute)
                        continue;

                    if (detail::articleEntryReference(*document))
                        continue;

                    if (!ids_.contains(*document->fragment))
                        return std::unexpected(SourceError{
                            "Unknown page fragment: " + *document->fragment, reference.source
                        });
                }

                return {};
            }

            bool for_each(pugi::xml_node& node) override
            {
                const auto offset = node.offset_debug();
                source_ = offset < 0 ? std::nullopt : std::optional(SourceRange{static_cast<size_t>(offset), 0});
                if (const auto id = node.attribute("id"))
                {
                    if (!ids_.insert(id.value()).second)
                        error = SourceError{"Duplicate XML id within a page", source_};
                    if (const auto target = detail::EntryReference::parse(id.value()))
                    {
                        if (target->pageId != page_)
                            error = SourceError{"Entry anchor belongs to another page", source_};
                        else if (id.value() != target->toString())
                            error = SourceError{
                                "Noncanonical entry anchor: " + std::string(id.value()) + ". Expected " +
                                target->toString(),
                                source_
                            };
                        else
                            result.anchors.insert(*target);
                    }
                }

                if (const auto style = node.attribute("style"))
                    css(style.value(), CssInput::Declarations);

                if (std::string_view(node.name()) == "style")
                {
                    result.diagnostics.push_back(
                        {
                            DiagnosticKind::IgnoredStyleElement, source_,
                            "Dictionaries ignores <style> elements. Link a stylesheet or use inline style attributes."
                        });
                    css(node.child_value(), CssInput::Stylesheet);
                }

                if (const auto source = node.attribute("src"))
                    reference(source.value(), true);
                if (const auto link = node.attribute("href"))
                    reference(link.value(), std::string_view(node.name()) == "link");

                return !error;
            }

        private:
            void css(std::string_view text, const CssInput input)
            {
                auto analysis = analyzeCss(text, input);
                if (!analysis)
                {
                    error = analysis.error();
                    error->source = source_;
                    return;
                }
                // XML decoding can change byte positions, so embedded CSS refers to its containing element.
                for (auto& reference : analysis->references)
                {
                    reference.source = source_;
                    result.references.push_back(std::move(reference));
                }
                for (auto& diagnostic : analysis->diagnostics)
                {
                    diagnostic.source = source_;
                    result.diagnostics.push_back(std::move(diagnostic));
                }
            }

            void reference(std::string_view value, bool asset)
            {
                if (!asset)
                    result.references.push_back(detail::linkReference(value, source_));
                else if (auto reference = detail::resourceReference(value, ResourceSyntax::XmlAttribute, source_))
                    result.references.push_back(std::move(*reference));
            }

            uint32_t page_;
            std::optional<SourceRange> source_;
            std::set<std::string> ids_;
        };
    }

    AnalysisResult<ArticleAnalysis> analyzeArticle(const uint32_t page, std::string_view xhtml)
    {
        if (auto valid = WriteValidation::entryId({page, 0}); !valid)
            return std::unexpected(SourceError{valid.error(), {}});
        if (auto valid = WriteValidation::xmlText(xhtml); !valid)
            return std::unexpected(SourceError{valid.error(), {}});

        pugi::xml_document document;
        const auto parsed = document.load_buffer(xhtml.data(), xhtml.size(), pugi::parse_default, pugi::encoding_utf8);
        if (!parsed)
            return std::unexpected(SourceError{
                "Page must be a well-formed UTF-8 XHTML document with an html root: " + std::string(
                    parsed.description()),
                SourceRange{static_cast<size_t>(std::max<ptrdiff_t>(0, parsed.offset)), 0}
            });

        if (std::string_view(document.document_element().name()) != "html")
            return std::unexpected(SourceError{
                "Page must be a well-formed UTF-8 XHTML document with an html root", {}
            });

        for (const auto node : document.children())
            if (node.type() == pugi::node_element && node != document.document_element())
                return std::unexpected(SourceError{
                    "Page must contain exactly one document element",
                    SourceRange{static_cast<size_t>(node.offset_debug()), 0}
                });

        ArticleWalker walker(page);
        const auto root = document.document_element();
        if (std::string_view(root.attribute("xmlns").value()) != "http://www.w3.org/1999/xhtml" ||
            std::string_view(root.attribute("hmhtml").value()) != "5")
            walker.result.diagnostics.push_back({
                DiagnosticKind::NativeMarkup,
                SourceRange{static_cast<size_t>(std::max<ptrdiff_t>(0, root.offset_debug())), 0},
                "Native articles should declare the XHTML namespace and hmhtml=\"5\"."
            });

        document.traverse(walker);
        if (walker.error)
            return std::unexpected(*walker.error);
        if (auto valid = walker.validateFragments(); !valid)
            return std::unexpected(valid.error());

        return std::move(walker.result);
    }
}
