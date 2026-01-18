//
// Caoimheにより 2026/01/16 に作成されました。
//

#pragma once

#include <cstdint>
#include <string_view>
#include <span>


namespace monokakido::resource
{
    enum class CompressionFormat : uint16_t
    {
        Uncompressed = 0,
        Zlib = 1
    };

    // Resource item that can be returned to users
    struct ResourceItem
    {
        std::string_view id;
        std::span<const uint8_t> data;
    };
}