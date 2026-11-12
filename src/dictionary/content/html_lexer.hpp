#pragma once

#include "html_token.hpp"

namespace MKD::detail::html
{
    /** Reads static HTML syntax without constructing a rendering tree or resolving resources. */
    class Lexer
    {
    public:
        explicit Lexer(std::string_view input) : input_(input)
        {
        }

        content::AnalysisResult<Token> next();

    private:
        enum class State
        {
            Content,
            Attributes,
            RawText
        };

        content::AnalysisResult<Token> markup();
        content::AnalysisResult<Token> attribute();
        Token rawText();
        std::string_view name();
        void skipWhitespace();
        std::string_view input_;
        size_t position_ = 0;
        State state_ = State::Content;
        Token element_;
    };
}
