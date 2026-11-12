#include "html_lexer.hpp"
#include <algorithm>

namespace MKD::detail::html
{
    constexpr std::string_view whitespace = " \t\r\n\f";
    constexpr std::string_view nameDelimiters = " \t\r\n\f=/>\"'<";
    constexpr std::string_view unquotedValueDelimiters = " \t\r\n\f>";
    constexpr std::string_view endTagDelimiters = " \t\r\n\f/>";
    constexpr std::string_view rawTextElements[]{
        "script", "style", "textarea", "title",
        "xmp", "iframe", "noembed", "noframes"
    };

    void Lexer::skipWhitespace()
    {
        const auto next = input_.find_first_not_of(whitespace, position_);
        position_ = next == std::string_view::npos ? input_.size() : next;
    }

    std::string_view Lexer::name()
    {
        const auto start = position_;
        const auto end = input_.find_first_of(nameDelimiters, start);
        position_ = end == std::string_view::npos ? input_.size() : end;
        return input_.substr(start, position_ - start);
    }

    content::AnalysisResult<Token> Lexer::attribute()
    {
        const auto start = position_;
        const auto attributeName = name();
        if (attributeName.empty())
            return std::unexpected(content::SourceError{
                "Malformed HTML attribute limits reference coverage",
                content::SourceRange{start, 0}
            });

        skipWhitespace();
        std::string_view value;
        if (position_ < input_.size() && input_[position_] == '=')
        {
            ++position_;
            skipWhitespace();
            const char quote = position_ < input_.size() ? input_[position_] : 0;
            if (quote == '\'' || quote == '"')
            {
                const auto begin = ++position_;
                const auto end = input_.find(quote, begin);
                if (end == std::string_view::npos)
                    return std::unexpected(content::SourceError{
                        "Unterminated HTML attribute limits reference coverage",
                        content::SourceRange{start, 0}
                    });
                value = input_.substr(begin, end - begin);
                position_ = end + 1;
            }
            else
            {
                const auto begin = position_;
                const auto end = input_.find_first_of(unquotedValueDelimiters, begin);
                position_ = end == std::string_view::npos ? input_.size() : end;
                value = input_.substr(begin, position_ - begin);
            }
        }
        return Token{TokenKind::Attribute, attributeName, value, {start, position_ - start}};
    }

    content::AnalysisResult<Token> Lexer::markup()
    {
        const auto start = position_++;
        if (input_.substr(start).starts_with("<!--"))
        {
            const auto end = input_.find("-->", position_);
            if (end == std::string_view::npos)
                return std::unexpected(content::SourceError{
                    "Unterminated HTML comment limits reference coverage",
                    content::SourceRange{start, 0}
                });

            position_ = end + 3;
            return Token{TokenKind::Text, {}, {}, {start, position_ - start}};
        }

        if (position_ == input_.size())
            return Token{TokenKind::Text, {}, "<", {start, 1}};

        if (input_[position_] == '!' || input_[position_] == '?')
        {
            char quote = 0;
            while (position_ < input_.size())
            {
                const char c = input_[position_++];
                if (quote)
                {
                    if (c == quote)
                        quote = 0;
                }
                else if (c == '\'' || c == '"')
                    quote = c;
                else if (c == '>')
                    return Token{TokenKind::Text, {}, {}, {start, position_ - start}};
            }

            return std::unexpected(content::SourceError{
                "Unterminated HTML declaration limits reference coverage",
                content::SourceRange{start, 0}
            });
        }

        const bool close = input_[position_] == '/';
        if (close)
            ++position_;

        const auto tag = name();
        if (tag.empty())
            return Token{TokenKind::Text, {}, "<", {start, 1}};

        if (close)
        {
            const auto end = input_.find('>', position_);
            position_ = end == std::string_view::npos ? input_.size() : end + 1;
            return Token{TokenKind::EndElement, tag, {}, {start, position_ - start}};
        }

        state_ = State::Attributes;
        element_ = {TokenKind::StartElement, tag, {}, {start, position_ - start}};
        return element_;
    }

    Token Lexer::rawText()
    {
        const auto start = position_;
        size_t close = position_;
        while ((close = input_.find("</", close)) != std::string_view::npos)
        {
            const auto candidate = close + 2;
            Token tag{TokenKind::EndElement, input_.substr(candidate, element_.name.size()), {}, {}};
            const auto end = candidate + element_.name.size();
            if (tag.named(element_.name) && (end == input_.size() || endTagDelimiters.contains(input_[end])))
                break;

            close += 2;
        }

        position_ = close == std::string_view::npos ? input_.size() : close;
        state_ = State::Content;
        return {TokenKind::Text, element_.name, input_.substr(start, position_ - start), {start, position_ - start}};
    }

    content::AnalysisResult<Token> Lexer::next()
    {
        if (state_ == State::Attributes)
        {
            skipWhitespace();
            if (position_ == input_.size())
                return std::unexpected(
                    content::SourceError{"Unterminated HTML tag limits reference coverage", element_.source});

            if (input_.substr(position_).starts_with("/>"))
                ++position_;
            if (input_[position_] != '>')
                return attribute();

            ++position_;
            state_ = State::Content;
            if (std::ranges::any_of(rawTextElements, [&](auto name) { return element_.named(name); }))
                state_ = State::RawText;
        }

        if (state_ == State::RawText)
            return rawText();
        if (position_ == input_.size())
            return Token{};
        if (input_[position_] == '<')
            return markup();

        const auto start = position_;
        const auto end = input_.find('<', start);
        position_ = end == std::string_view::npos ? input_.size() : end;
        return Token{TokenKind::Text, {}, input_.substr(start, position_ - start), {start, position_ - start}};
    }
}
