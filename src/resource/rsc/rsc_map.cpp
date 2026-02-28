//
// kiwakiwaaにより 2026/01/29 に作成されました。
//

#include "MKD/resource/rsc/rsc_map.hpp"


namespace MKD
{
    void MapHeader::swapEndianness() noexcept
    {
        version = std::byteswap(version);
        recordCount = std::byteswap(recordCount);
    }

    void MapRecord::swapEndianness() noexcept
    {
        chunkGlobalOffset = std::byteswap(chunkGlobalOffset);
        itemOffset = std::byteswap(itemOffset);
    }
}