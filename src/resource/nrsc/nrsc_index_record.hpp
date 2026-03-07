//
// kiwakiwaaにより 2026/01/17 に作成されました。
//

#pragma once

#include "MKD/resource/common.hpp"

#include <format>

namespace MKD
{
    /**
     * Represents a single entry in the .nrsc index
     *
     * This class loads and provides access to the index that maps string IDs
     * to resources stored across multiple numbered .nrsc files. Records are
     * stored sorted by ID string to enable binary search lookups.
     */
    struct NrscIndexRecord : BinaryStruct<NrscIndexRecord>
    {
        uint16_t format;            // Compression: 0=uncompressed, 1=zlib
        uint16_t fileSequence;      // Which numbered .nrsc file (0.nrsc, 1.nrsc, etc.)
        uint32_t idStringOffset;    // Byte offset into the ID strings region
        uint32_t fileOffset;        // Byte offset within the target .nrsc file
        uint32_t length;            // size of the resource data

        [[nodiscard]] bool isCompressed() const noexcept;
        [[nodiscard]] size_t fileSeq() const noexcept;
        [[nodiscard]] size_t idOffset() const noexcept;
        [[nodiscard]] uint64_t offset() const noexcept;
        [[nodiscard]] size_t len() const noexcept;

        void swapEndianness() noexcept;

        [[nodiscard]] std::string formattedSize() const noexcept;
    };

    static_assert(sizeof(NrscIndexRecord) == 16, "NrscIndexRecord should be 16 bytes long");
}


template<>
struct std::formatter<MKD::NrscIndexRecord>
{
    static constexpr auto parse(const std::format_parse_context& ctx)
    {
        const auto it = ctx.begin();
        if (it == ctx.end() || *it == '}')
            return it;

        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format specifier for NrscIndexRecord");

        return ctx.begin();
    }

    static auto format(const MKD::NrscIndexRecord& record, std::format_context& ctx)
    {
        const char compression = record.isCompressed() ? 'Z' : 'U';
        return std::format_to(ctx.out(),
            "{}.nrsc@{} ({}) ({})",
            record.fileSequence,
            record.fileOffset,
            record.formattedSize(),
            compression == 'U' ? "Uncompressed" : "Zlib");
    }
};
