#pragma once

#include "MKD/dictionary/content_analysis.hpp"

namespace MKD::detail::html
{
    enum class TokenKind
    {
        End,
        StartElement,
        Attribute,
        EndElement,
        Text
    };

    /** Tokens borrow the input buffer. Attribute decoding returns an owned UTF-8 value. */
    struct Token
    {
        TokenKind kind = TokenKind::End;
        std::string_view name;
        std::string_view value;
        content::SourceRange source;
        bool named(std::string_view expected) const;
        Result<std::string> decodedValue() const;
    };
}
