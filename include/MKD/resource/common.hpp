//
// kiwakiwaaにより 2026/01/16 に作成されました。
//

#pragma once

#include <bit>
#include <filesystem>
#include <optional>
#include <string_view>

namespace fs = std::filesystem;

namespace MKD
{
    static_assert(std::endian::native == std::endian::little, "mkd-tools only supports little-endian platforms");

    namespace detail
    {
        std::optional<uint32_t> parseSequenceNumber(const fs::path& filename, std::string_view extension);
    }
}
