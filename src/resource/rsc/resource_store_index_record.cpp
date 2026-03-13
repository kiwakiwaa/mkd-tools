//
// kiwakiwaaにより 2026/01/29 に作成されました。
//

#include "resource_store_index_record.hpp"

namespace MKD
{
    uint32_t ResourceStoreIndexRecord::id() const noexcept
    {
        return itemId;
    }

    size_t ResourceStoreIndexRecord::mapIndex() const noexcept
    {
        return mapIdx;
    }
}
