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
    // Struct that provides automatic endiannessconversion
    template<typename Derived>
    struct BinaryStruct
    {
        void toLittleEndian() noexcept
        {
            if constexpr (std::endian::native == std::endian::big)
            {
                static_cast<Derived*>(this)->swapEndianness();
            }
        }

        auto operator<=>(const BinaryStruct&) const noexcept = default;
    };

    // Concept to verify that type has endianness support
    template<typename T>
    concept SwappableEndianness = requires(T t)
    {
        { t.swapEndianness() } -> std::same_as<void>;
        { t.toLittleEndian() } -> std::same_as<void>;
    };

    namespace detail
    {
        std::optional<uint32_t> parseSequenceNumber(const fs::path& filename, std::string_view extension);
    }
}
