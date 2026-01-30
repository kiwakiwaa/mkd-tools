//
// kiwakiwaaにより 2026/01/29 に作成されました。
//

#include "monokakido/resource/rsc/rsc_map.hpp"


namespace monokakido::resource
{
    void MapHeader::swapEndianness() noexcept
    {
        version = std::byteswap(version);
        recordCount = std::byteswap(recordCount);
    }

    void MapRecord::swapEndianness() noexcept
    {
        zOffset = std::byteswap(zOffset);
        ioffset = std::byteswap(ioffset);
    }
}