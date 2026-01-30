//
// kiwakiwaaにより 2026/01/29 に作成されました。
//

#pragma once

#include "monokakido/resource/common.hpp"

namespace monokakido::resource
{
    /**
     * MapHeader - contents.map File Header
     * =====================================
     *
     * Binary Layout (8 bytes):
     * ┌────────────────────────┬────────────────────────┐
     * │ version (4 bytes)      │ recordCount (4 bytes)  │
     * │ Format version         │ Number of MapRecords   │
     * └────────────────────────┴────────────────────────┘
     */
    struct MapHeader : BinaryStruct<MapHeader>
    {
        uint32_t version; // Format version identifier
        uint32_t recordCount; // Number of MapRecords in file

        void swapEndianness() noexcept;
    };

    struct MapRecord : BinaryStruct<MapRecord>
    {
        uint32_t zOffset; // Global offset to compressed chunk
        uint32_t ioffset; // Offset within decompressed chunk

        // if iOffset is set to 0xFFFFFFFF, it means that compressed chunks are not used and the data
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
