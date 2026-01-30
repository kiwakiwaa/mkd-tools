//
// kiwakiwaaにより 2026/01/29 に作成されました。
//

#pragma once

#include <expected>
#include <span>
#include <string>
#include <string_view>


namespace monokakido::resource
{
    class XmlView
    {
    public:

        explicit XmlView(std::span<const uint8_t> data);

        [[nodiscard]] std::expected<std::string_view, std::string> asStringView() const;

        [[nodiscard]] std::expected<void, std::string> validate() const;

        [[nodiscard]] std::span<const uint8_t> data() const noexcept;

    private:

        std::span<const uint8_t> data_;

    };
}