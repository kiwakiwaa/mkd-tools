//
// kiwakiwaaにより 2026/01/29 に作成されました。
//

#pragma once

#include "MKD/resource/common.hpp"

#include <format>

namespace MKD
{
    /**
     * MapHeader - contents.map File Header
     */
    struct MapHeader : BinaryStruct<MapHeader>
    {
        uint32_t version; // Format version identifier. 0x01 means the contents have encryption applied
        uint32_t recordCount; // Number of MapRecords in file

        void swapEndianness() noexcept;
    };

    struct MapRecord : BinaryStruct<MapRecord>
    {
        uint32_t chunkGlobalOffset; // Global offset to compressed chunk
        uint32_t itemOffset; // Offset within decompressed chunk

        // if itemOffset is set to 0xFFFFFFFF, it means that compressed chunks are not used and the data
        // should be read directly from the global offset. This is the case for fonts

        /**
         * Convert between endianness formats
         * Called automatically on big-endian systems
         */
        void swapEndianness() noexcept;


        /**
         * Default comparison operators
         * Enables sorting and binary search by all fields
         */
        auto operator<=>(const MapRecord&) const noexcept = default;
    };

    static_assert(sizeof(MapRecord) == 8, "MapRecord must be 8 bytes");
}

template<>
struct std::formatter<MKD::MapRecord>
{
    static constexpr auto parse(const std::format_parse_context& ctx)
    {
        const auto it = ctx.begin();
        if (it == ctx.end() || *it == '}')
            return it;

        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format specifier for MapRecord");

        return ctx.begin();
    }

    static auto format(const MKD::MapRecord& record, std::format_context& ctx)
    {
        return std::format_to(ctx.out(),
                              "chunkGlobalOffset: {:10} | itemOffset: {:6}",
                              record.chunkGlobalOffset,
                              record.itemOffset);
    }
};
