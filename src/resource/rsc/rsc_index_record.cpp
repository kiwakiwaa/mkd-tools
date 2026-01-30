//
// kiwakiwaaにより 2026/01/29 に作成されました。
//

#include "monokakido/resource/rsc/rsc_index_record.hpp"


namespace monokakido::resource
{
    uint32_t IdxRecord::id() const noexcept
    {
        return itemId;
    }

    size_t IdxRecord::mapIndex() const noexcept
    {
        return mapIdx;
    }

    void IdxRecord::swapEndianness() noexcept
    {
        itemId = std::byteswap(itemId);
        mapIdx = std::byteswap(mapIdx);
    }
}