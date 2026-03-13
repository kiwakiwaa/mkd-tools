//
// kiwakiwaaにより 2026/01/17 に作成されました。
//

#include "named_resource_store_index_record.hpp"

namespace MKD
{
    bool NamedResourceStoreIndexRecord::isCompressed() const noexcept
    {
        return format == 1;
    }


    size_t NamedResourceStoreIndexRecord::fileSeq() const noexcept
    {
        return fileSequence;
    }

    size_t NamedResourceStoreIndexRecord::idOffset() const noexcept
    {
        return idStringOffset;
    }

    uint64_t NamedResourceStoreIndexRecord::offset() const noexcept
    {
        return fileOffset;
    }

    size_t NamedResourceStoreIndexRecord::len() const noexcept
    {
        return length;
    }

    std::string NamedResourceStoreIndexRecord::formattedSize() const noexcept
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
