//
// kiwakiwaaにより 2026/02/12 に作成されました。
//

#pragma once

#include "MKD/resource/common.hpp"

namespace MKD
{
    /**
     * Records are sorted by this composite key
     */
    struct HeadlineEntryId : BinaryStruct<HeadlineEntryId>
    {
        uint32_t pageId;
        uint16_t itemId;

        void swapEndianness() noexcept
        {
            pageId = std::byteswap(pageId);
            itemId = std::byteswap(itemId);
        }
    };


    /**
     * Headlines Record
     *
     * Each record maps an entry ID to string offsets within the strings region.
     * The full headline is constructed as: prefix + headline + suffix
     *
     * Layout (20 bytes, or more if stride > 20):
     *  ┌──────────────────────────────────────────────────────┐
     *  │ entryIdLow          (4 bytes, uint32)                │
     *  │ entryIdHigh         (2 bytes, uint16)                │
     *  │ reserved            (2 bytes)                        │
     *  │ headlineOffset      (4 bytes, uint32)  → strings     │
     *  │ prefixOffset        (4 bytes, uint32)  → strings     │
     *  │ suffixOffset        (4 bytes, uint32)  → strings     │
     *  ├──────────────────────────────────────────────────────┤
     *  │ sortHeadlineOffset  (4 bytes, uint32)  [if stride≥21]│
     *  └──────────────────────────────────────────────────────┘
     *
     * String offsets are byte offsets into the UTF-16LE strings region.
     * An offset of 0 means the field is absent (prefix/suffix are optional).
     */
    struct HeadlineRecord : BinaryStruct<HeadlineRecord>
    {
        uint32_t entryIdLow;
        uint16_t entryIdHigh;
        uint16_t reserved;
        uint32_t headlineOffset;
        uint32_t prefixOffset;       // 0 = no prefix
        uint32_t suffixOffset;       // 0 = no suffix

        [[nodiscard]] HeadlineEntryId entryId() const noexcept
        {
            return { .pageId = entryIdLow, .itemId = entryIdHigh };
        }

        void swapEndianness() noexcept
        {
            entryIdLow = std::byteswap(entryIdLow);
            entryIdHigh = std::byteswap(entryIdHigh);
            headlineOffset = std::byteswap(headlineOffset);
            prefixOffset = std::byteswap(prefixOffset);
            suffixOffset = std::byteswap(suffixOffset);
        }
    };

    static_assert(sizeof(HeadlineRecord) == 20);

}
