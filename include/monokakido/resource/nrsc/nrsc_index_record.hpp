//
// Caoimheにより 2026/01/17 に作成されました。
//

#pragma once

#include "../common.hpp"

#include <iostream>
#include <format>


namespace monokakido::resource
{
    /**
     * Represents a single entry in the .nrsc index
     *
     * This class loads and provides access to the index that maps string IDs
     * to resources stored across multiple numbered .nrsc files. Records are
     * stored sorted by ID string to enable binary search lookups.
     */
    struct NrscIndexRecord
    {
        uint16_t format;            // Compression: 0=uncompressed, 1=zlib
        uint16_t fileSequence;      // Which numbered .nrsc file (0.nrsc, 1.nrsc, etc.)
        uint32_t idStringOffset;    // Byte offset into the ID strings region
        uint32_t fileOffset;        // Byte offset within the target .nrsc file
        uint32_t length;            // size of the resource data

        [[nodiscard]] CompressionFormat compressionFormat() const;
        [[nodiscard]] size_t fileSeq() const noexcept;
        [[nodiscard]] size_t idOffset() const noexcept;
        [[nodiscard]] uint64_t offset() const noexcept;
        [[nodiscard]] size_t len() const noexcept;

        void toLittleEndian() noexcept;

        [[nodiscard]] std::string formattedSize() const noexcept;
    };

    static_assert(sizeof(NrscIndexRecord) == 16, "NrscIndexRecord should be 16 bytes long");
}


template<>
struct std::formatter<monokakido::resource::NrscIndexRecord>
{
    enum class Style { compact, detailed, hex };

    Style style_ = Style::compact;

    constexpr auto parse(const std::format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it == ctx.end() || *it == '}')
            return it;

        if (*it == '.')
        {
            ++it;
            if (it != ctx.end())
            {
                switch (*it)
                {
                    case 'c': style_ = Style::compact;
                        break;
                    case 'd': style_ = Style::detailed;
                        break;
                    case 'h': style_ = Style::hex;
                        break;
                    default: throw std::format_error(
                            std::format("Invalid format specifier for NrscIndexRecord: {}", *it));
                }
                ++it;
            }
        }

        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format specifier for NrscIndexRecord");

        return ctx.begin();
    }

    auto format(const monokakido::resource::NrscIndexRecord& record, std::format_context& ctx) const
    {
        const char compression = record.compressionFormat() == monokakido::resource::CompressionFormat::Uncompressed ? 'U' : 'Z';

        switch (style_)
        {
            case Style::compact:
                return std::format_to(ctx.out(),
                    "{}.nrsc@{:#x} ({}) ({})",
                    record.fileSequence,
                    record.fileOffset,
                    record.formattedSize(),
                    compression);

            case Style::detailed:
                return std::format_to(ctx.out(),
                    "Format: {} | File: {}.nrsc | Offset: {:#010x} | Length: {} bytes | ID Offset: {:#x}",
                    compression == 'U' ? "Uncompressed" : "Zlib",
                    record.fileSequence,
                    record.fileOffset,
                    record.length,
                    record.idStringOffset);

            case Style::hex:
                return std::format_to(ctx.out(),
                    "fmt={:#06x} seq={:#06x} idOff={:#010x} fileOff={:#010x} len={:#010x}",
                    record.format,
                    record.fileSequence,
                    record.idStringOffset,
                    record.fileOffset,
                    record.length);
        }

        return ctx.out();
    }
};
