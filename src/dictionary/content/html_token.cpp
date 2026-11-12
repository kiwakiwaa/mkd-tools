#include "html_token.hpp"
#include "unicode/utf8.hpp"
#include <algorithm>
#include <charconv>

namespace MKD::detail::html
{
    bool Token::named(std::string_view expected) const
    {
        const auto fold = [](const unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
        return std::ranges::equal(name, expected, {}, fold, fold);
    }

    constexpr std::pair<std::string_view, char32_t> namedEntities[]{
        {"amp", '&'}, {"quot", '"'}, {"apos", '\''},
        {"lt", '<'}, {"gt", '>'}, {"nbsp", 0xA0},
        {"copy", 0xA9}, {"reg", 0xAE}
    };

    static Result<char32_t> decodeNumericReference(std::string_view digits)
    {
        const bool hexadecimal = digits.starts_with('x') || digits.starts_with('X');
        if (hexadecimal)
            digits.remove_prefix(1);

        uint32_t codepoint{};
        const auto [ptr, ec] =
            std::from_chars(digits.data(), digits.data() + digits.size(), codepoint, hexadecimal ? 16 : 10);

        if (ec != std::errc{} || ptr != digits.data() + digits.size() || !codepoint ||
            !unicode::isUnicodeScalarValue(codepoint))
            return std::unexpected("Invalid numeric HTML character reference");

        return codepoint;
    }

    static Result<std::optional<char32_t>> decodeCharacterReference(std::string_view name)
    {
        if (name.starts_with('#'))
            return decodeNumericReference(name.substr(1))
                .transform([](char32_t value) { return std::optional(value); });

        const auto found = std::ranges::find(namedEntities, name, &std::pair<std::string_view, char32_t>::first);
        if (found != std::end(namedEntities))
            return found->second;

        const bool named =
            !name.empty() &&
            std::ranges::all_of(name, [](unsigned char c)
            {
                return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
            });

        if (named)
            return std::unexpected("Unchecked HTML named character reference: &" + std::string(name) + ";");

        return std::nullopt;
    }

    Result<std::string> Token::decodedValue() const
    {
        std::string result;
        size_t position = 0;
        while (position < value.size())
        {
            const auto ampersand = value.find('&', position);
            result += value.substr(position, ampersand - position);
            if (ampersand == std::string_view::npos)
                break;

            const auto end = value.find(';', ampersand + 1);
            if (end == std::string_view::npos)
            {
                result += value.substr(ampersand);
                break;
            }

            auto decoded = decodeCharacterReference(value.substr(ampersand + 1, end - ampersand - 1));
            if (!decoded)
                return std::unexpected(decoded.error());
            if (*decoded)
                unicode::appendUtf8(result, **decoded);
            else
                result += '&';

            position = *decoded ? end + 1 : ampersand + 1;
        }
        return result;
    }
}
