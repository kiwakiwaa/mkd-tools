//
// kiwakiwaaにより 2026/02/07 に作成されました。
//

#pragma once

#include "MKD/result.hpp"

#include <cstdint>
#include <expected>
#include <span>
#include <vector>


namespace MKD
{
    // Reference to a dictionary entry
    struct EntryId
    {
        uint32_t pageId; // Page number in dictionary content
        uint16_t itemId; // Entry ID within page (HTML element id)

        auto operator<=>(const EntryId&) const = default;
    };

    // Variable-length encoding
    //
    // Each entry is bit-packed with a leading flags byte:
    //   0x01: page is 1 byte
    //   0x02: page is 2 bytes (big-endian)
    //   0x04: page is 3 bytes (big-endian)
    //   0x08: type/filter byte present (used for search filtering, not part of the entry ID)
    //   0x10: item is 1 byte
    //   0x20: item is 2 bytes (big-endian)
    //   0x40: extra is 1 byte
    //   0x80: extra is 2 bytes (big-endian)
    struct DecodedEntry
    {
        uint32_t page;
        uint16_t item;
        uint16_t extra;     // Encoded via flags 0x40/0x80
        uint8_t  type;      // Filter byte (flag 0x08)
        bool     hasType;
        size_t   bytesConsumed;
    };

    /**
     * Decode a single variable-length entry from raw bytes.
     */
    [[nodiscard]] Result<DecodedEntry> decodeKeystoreEntry(std::span<const uint8_t> data);

    /**
     * Decode all entry ids from a counted block.
     *
     * Block format: uint16_le count | encoded entries...
     *
     * @param data  Span starting at the uint16 count field
     * @return Decoded entry ids, or an error
     */
    [[nodiscard]] Result<std::vector<EntryId>> decodeEntryIds(std::span<const uint8_t> data);
}
