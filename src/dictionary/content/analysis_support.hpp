//
// kiwakiwaaにより 2026/10/24 に作成されました。
//

#pragma once

#include "MKD/dictionary/content_analysis.hpp"
#include <optional>
#include <utility>
#include <charconv>
#include "content_markup.hpp"

namespace MKD::detail
{
    class SourceMap
    {
    public:
        explicit SourceMap(std::string_view input);
        [[nodiscard]] std::string describe(std::string_view origin,
                                           std::optional<content::SourceRange> range) const;

        [[nodiscard]] std::pair<size_t, size_t> lineColumn(size_t offset) const;

    private:
        size_t size_;
        std::vector<size_t> lines_{0};
    };

    inline std::optional<content::EntryReference> entryReference(std::string_view value)
    {
        if (const auto local = EntryReference::parse(value))
            return content::EntryReference{*local, {}};

        if (const auto dash = value.rfind('-'); dash != std::string_view::npos)
            if (const auto local = EntryReference::parse(value.substr(0, dash)))
            {
                uint16_t dictionary{};
                const auto parsed = std::from_chars(value.data() + dash + 1, value.data() + value.size(), dictionary);
                if (parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size())
                    return content::EntryReference{*local, dictionary};
            }
        return {};
    }

    inline std::optional<content::EntryReference> articleEntryReference(const content::DocumentReference& document)
    {
        if (!document.path.empty() || !document.fragment || document.syntax != content::ResourceSyntax::XmlAttribute)
            return {};

        auto entry = entryReference(*document.fragment);
        return entry && entry->entryId.pageId ? entry : std::nullopt;
    }

    inline content::Reference linkReference(std::string_view value, std::optional<content::SourceRange> source)
    {
        if (auto entry = entryReference(value))
            return {*entry, source};

        if (value.contains(':') || value.starts_with("//"))
            return {content::ExternalReference{std::string(value)}, source};

        const auto hash = value.find('#');
        return {
            content::DocumentReference{
                std::string(value.substr(0, hash)),
                hash == std::string_view::npos ? std::nullopt : std::optional(std::string(value.substr(hash + 1)))
            },
            source
        };
    }

    inline std::optional<content::Reference> resourceReference(std::string_view value,
                                                               const content::ResourceSyntax syntax,
                                                               std::optional<content::SourceRange> source)
    {
        if (value.empty())
            return std::nullopt;

        if (value.contains(':') || value.starts_with("//"))
            return content::Reference{content::ExternalReference{std::string(value)}, source};

        if (value.starts_with('#'))
            return content::Reference{content::DocumentReference{{}, std::string(value.substr(1)), syntax}, source};

        return content::Reference{content::AssetReference{std::string(value), syntax}, source};
    }
}
