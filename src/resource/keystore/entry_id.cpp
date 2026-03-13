//
// kiwakiwaaにより 2026/02/11 に作成されました。
//

#include "MKD/resource/entry_id.hpp"

#include <cstring>
#include <format>

namespace MKD
{
    Result<DecodedEntry> decodeKeystoreEntry(
        const std::span<const uint8_t> data)
    {
        if (data.empty())
            return std::unexpected("Empty data for keystore entry");

        const uint8_t flags = data[0];
        size_t offset = 1;
        DecodedEntry entry{};

        if (flags & 0x01)
        {
            if (offset + 1 > data.size())
                return std::unexpected("Truncated page field");
            entry.page = data[offset];
            offset += 1;
        }
        else if (flags & 0x02)
        {
            if (offset + 2 > data.size())
                return std::unexpected("Truncated page field");
            entry.page = (static_cast<uint32_t>(data[offset]) << 8) |
                         data[offset + 1];
            offset += 2;
        }
        else if (flags & 0x04)
        {
            if (offset + 3 > data.size())
                return std::unexpected("Truncated page field");
            entry.page = (static_cast<uint32_t>(data[offset]) << 16) |
                         (static_cast<uint32_t>(data[offset + 1]) << 8) |
                         data[offset + 2];
            offset += 3;
        }

        if (flags & 0x10)
        {
            if (offset + 1 > data.size())
                return std::unexpected("Truncated item field");
            entry.item = data[offset];
            offset += 1;
        }
        else if (flags & 0x20)
        {
            if (offset + 2 > data.size())
                return std::unexpected("Truncated item field");
            entry.item = (static_cast<uint16_t>(data[offset]) << 8) |
                         data[offset + 1];
            offset += 2;
        }

        if (flags & 0x40)
        {
            if (offset + 1 > data.size())
                return std::unexpected("Truncated extra field");
            entry.extra = data[offset];
            offset += 1;
        }
        else if (flags & 0x80)
        {
            if (offset + 2 > data.size())
                return std::unexpected("Truncated extra field");
            entry.extra = (static_cast<uint16_t>(data[offset]) << 8) |
                          data[offset + 1];
            offset += 2;
        }

        if (flags & 0x08)
        {
            if (offset + 1 > data.size())
                return std::unexpected("Truncated type field");
            entry.type = data[offset];
            offset += 1;
            entry.hasType = true;
        }

        entry.bytesConsumed = offset;
        return entry;
    }

    Result<std::vector<EntryId>> decodeEntryIds(const std::span<const uint8_t> data)
    {
        if (data.size() < 2)
            return std::unexpected(std::format(
                "Entry id block too small: {} bytes", data.size()));

        uint16_t count;
        std::memcpy(&count, data.data(), sizeof(uint16_t));

        auto remaining = data.subspan(2);
        std::vector<EntryId> refs;
        refs.reserve(count);

        for (uint16_t i = 0; i < count; ++i)
        {
            auto decoded = decodeKeystoreEntry(remaining);
            if (!decoded)
                return std::unexpected(std::format(
                    "Failed to decode entry {}/{}: {}", i, count, decoded.error()));

            if (decoded->bytesConsumed > remaining.size())
                return std::unexpected("Entry overflows entry id data block");

            refs.push_back({decoded->page, decoded->item});
            remaining = remaining.subspan(decoded->bytesConsumed);
        }

        return refs;
    }
}
