//
// kiwakiwaaにより 2026/01/29 に作成されました。
//

#include "monokakido/resource/xml_view.hpp"

#include <format>
#include <utf8.h>

namespace monokakido::resource
{

    XmlView::XmlView(const std::span<const uint8_t> data)
        : data_(data)
    {
    }


    std::expected<std::string_view, std::string> XmlView::asStringView() const
    {
        if (auto validationResult = validate(); !validationResult)
            return std::unexpected(validationResult.error());

        return std::string_view(
            reinterpret_cast<const char*>(data_.data()),
            data_.size()
        );
    }


    std::expected<void, std::string> XmlView::validate() const
    {
        const auto* begin = data_.data();
        const auto* end = begin + data_.size();

        if (const auto* it = utf8::find_invalid(begin, end); it != end)
        {
            const size_t offset = it - begin;
            return std::unexpected(
                std::format("Invalid UTF-8 sequence at byte offset {}", offset)
            );
        }

        return {};
    }


    std::span<const uint8_t> XmlView::data() const noexcept
    {
        return data_;
    }

}