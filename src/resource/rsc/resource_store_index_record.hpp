//
// kiwakiwaaにより 2026/01/29 に作成されました。
//

#pragma once

#include "MKD/resource/common.hpp"

namespace MKD
{
    /**
     * IdxRecord - ID to Index Mapping (contents.idx)
     *
     * Maps arbitrary item IDs to sequential map indices. This indirection
     * allows dictionaries to use non-sequential numbering while keeping the underlying MapRecords sequential.
     */
    struct ResourceStoreIndexRecord
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
         * Default comparison operators
         * Enables sorting and binary search by all fields
         */
        auto operator<=>(const ResourceStoreIndexRecord&) const noexcept = default;
    };
    static_assert(sizeof(ResourceStoreIndexRecord) == 8, "IdxRecord must be 8 bytes");
}
