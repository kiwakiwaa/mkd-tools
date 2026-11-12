//
// kiwakiwaaにより 2026/10/24 に作成されました。
//

#include "MKD/dictionary/content_analysis.hpp"
#include "content_markup.hpp"
#include "io/write_validation.hpp"
#include "unicode/utf8.hpp"
#include <algorithm>

namespace MKD::content
{
    using detail::HeadlineToken;
    namespace monokakido = detail::monokakido;
    namespace unicode = detail::unicode;

    namespace
    {
        AnalysisResult<void> collect(std::string_view text, size_t offset, Analysis& result)
        {
            size_t position = 0;
            for (;;)
            {
                const auto begin = text.find('{', position);
                if (begin == std::string_view::npos)
                    return {};

                auto token = HeadlineToken::readLeading(text.substr(begin));
                if (!token)
                    return std::unexpected(SourceError{token.error(), SourceRange{offset + begin, 0}});

                const SourceRange source{offset + begin, token->source.size()};
                const auto element = token->element();
                if (const auto gaiji = std::get_if<Gaiji>(&element))
                    result.references.push_back({AssetReference{gaiji->resource.path}, source});
                else if (const auto link = std::get_if<EntryLink>(&element))
                    result.references.push_back({EntryReference{link->target, {}}, source});
                else if (std::holds_alternative<HeadlineMarkup>(element))
                {
                    const auto name = token->fields.front();
                    if (name == monokakido::headline::image && token->fields.size() >= 2)
                        result.references.push_back({AssetReference{std::string(token->fields[1])}, source});

                    if (name == monokakido::headline::reference && token->fields.size() >= 2)
                        if (const auto target = detail::EntryReference::parse(token->fields[1]))
                            result.references.push_back({EntryReference{*target, {}}, source});

                    if (std::ranges::find(monokakido::headline::commands, name) == monokakido::headline::commands.end()
                        &&
                        !name.starts_with("CID"))
                    {
                        if (name.contains('/'))
                            result.references.push_back({AssetReference{std::string(name)}, source});
                        else
                            result.references.push_back({GaijiReference{std::string(name)}, source});
                    }

                    for (const auto field : token->fields)
                        if (field.contains('{'))
                            if (auto nested = collect(field, offset + static_cast<size_t>(field.data() - text.data()),
                                                      result);
                                !nested)
                                return nested;
                }
                position = begin + token->source.size();
            }
        }
    }

    AnalysisResult<Analysis> analyzeHeadline(std::u16string_view text)
    {
        if (auto valid = detail::WriteValidation::text(text, true); !valid)
            return std::unexpected(SourceError{valid.error(), {}});

        const auto utf8 = unicode::toUtf8(text);
        Analysis result;
        if (auto collected = collect(utf8, 0, result); !collected)
            return std::unexpected(collected.error());

        return result;
    }
}
