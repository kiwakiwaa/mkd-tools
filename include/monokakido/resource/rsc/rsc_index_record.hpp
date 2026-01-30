//
// kiwakiwaaにより 2026/01/29 に作成されました。
//

#pragma once

#include "monokakido/resource/common.hpp"


namespace monokakido::resource
{
    /**
     * IdxRecord - ID to Index Mapping (contents.idx)
     * ===============================================
     *
     * Maps arbitrary item IDs to sequential map indices. This indirection
     * allows dictionaries to use non-sequential numbering while keeping the underlying MapRecords sequential.
     *
     * Binary Layout (8 bytes):
     * ┌────────────────────────┬────────────────────────┐
     * │ itemId (4 bytes)       │ mapIdx (4 bytes)       │
     * │ Custom entry ID        │ Index into map array   │
     * └────────────────────────┴────────────────────────┘
     */
    struct IdxRecord : BinaryStruct<IdxRecord>
    {
        uint32_t itemId; // Custom item ID
        uint32_t mapIdx; // Index into map file

        /**
         * Get the item ID
         * @return Item ID value
         */
        [[nodiscard]] uint32_t id() const noexcept;

        /**
         * Get the map index
         * @return Index into MapRecords array
         */
        [[nodiscard]] size_t mapIndex() const noexcept;

        /**
         * Convert between endianness formats
         * Called automatically on big-endian systems
         */
        void swapEndianness() noexcept;

        /**
         * Default comparison operators
         * Enables sorting and binary search by all fields
         */
        auto operator<=>(const IdxRecord&) const noexcept = default;
    };
    static_assert(sizeof(IdxRecord) == 8, "IdxRecord must be 8 bytes");
}
