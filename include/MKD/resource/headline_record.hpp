//
// kiwakiwaaにより 2026/02/12 に作成されました。
//

#pragma once

#include "MKD/resource/entry_id.hpp"

namespace MKD
{


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
    struct HeadlineRecord
    {
        uint32_t entryIdLow;
        uint16_t entryIdHigh;
        uint16_t reserved;
        uint32_t headlineOffset;
        uint32_t prefixOffset;       // 0 = no prefix
        uint32_t suffixOffset;       // 0 = no suffix

        [[nodiscard]] EntryId entryId() const noexcept
        {
            return { .pageId = entryIdLow, .itemId = entryIdHigh };
        }
    };

    static_assert(sizeof(HeadlineRecord) == 20);

}
