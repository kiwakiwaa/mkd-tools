//
// kiwakiwaaにより 2026/10/24 に作成されました。
//

#pragma once

#include "MKD/result.hpp"
#include "MKD/resource/retained_span.hpp"
#include <filesystem>
#include <map>
#include <span>
#include <string_view>
#include <vector>
#include <pugixml.hpp>

namespace MKD::detail
{
    /** Encoded product menus and information pages, checked before any finalisation output is emitted. */
    class PreparedProductFiles
    {
    public:
        explicit PreparedProductFiles(std::filesystem::path contents) : contents_(std::move(contents))
        {
        }

        Result<void> add(std::string_view path, std::span<const uint8_t> bytes);
        Result<void> add(std::string_view path, const pugi::xml_document& document);
        [[nodiscard]] bool contains(std::string_view path) const;
        Result<void> emit() const;
        Result<RetainedSpan> read(std::string_view path) const;

    private:
        std::filesystem::path contents_;
        std::map<std::string, std::vector<uint8_t>, std::less<>> files_;
    };
}
