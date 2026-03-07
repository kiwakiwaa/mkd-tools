//
// kiwakiwaaにより 2026/01/17 に作成されました。
//

#include "nrsc_index_record.hpp"

#include <bit>

namespace MKD
{
    bool NrscIndexRecord::isCompressed() const noexcept
    {
        return format == 1;;
    }


    size_t NrscIndexRecord::fileSeq() const noexcept
    {
        return fileSequence;
    }

    size_t NrscIndexRecord::idOffset() const noexcept
    {
        return idStringOffset;
    }

    uint64_t NrscIndexRecord::offset() const noexcept
    {
        return fileOffset;
    }

    size_t NrscIndexRecord::len() const noexcept
    {
        return length;
    }

    void NrscIndexRecord::swapEndianness() noexcept
    {
        format = std::byteswap(format);
        fileSequence = std::byteswap(fileSequence);
        idStringOffset = std::byteswap(idStringOffset);
        fileOffset = std::byteswap(fileOffset);
        length = std::byteswap(length);
    }

    std::string NrscIndexRecord::formattedSize() const noexcept
    {
        const auto bytes = static_cast<double>(length);

        if (bytes < 1024)
            return std::format("{} B", bytes);
        if (bytes < 1024 * 1024)
            return std::format("{:.1f} KB", bytes / 1024);
        if (bytes < 1024 * 1024 * 1024)
            return std::format("{:.1f} MB", bytes / 1024 / 1024);

        return std::format("{:.2f} GB", bytes / 1024 / 1024 / 1024);
    }
}







