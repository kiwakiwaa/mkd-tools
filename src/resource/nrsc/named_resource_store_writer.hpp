//
// kiwakiwaaにより 2026/03/07 に作成されました。
//

#pragma once

#include "MKD/result.hpp"
#include "resource/detail/sequential_blob_writer.hpp"

#include <string>
#include <vector>

namespace MKD
{
    class NamedResourceStoreWriter
    {
    public:
        struct Options
        {
            ResourceType type;
            size_t maxFileSize = 33 * 1024 * 1024;
            int compressionLevel = 0;
        };


        static Result<NamedResourceStoreWriter> create(const fs::path& directoryPath, const Options& options);

        Result<void> add(std::string_view id, std::vector<uint8_t> data, int compressionLevel = 0);

        Result<void> finalize();

        [[nodiscard]] size_t size() const noexcept;

        [[nodiscard]] bool empty() const noexcept;

    private:
        explicit NamedResourceStoreWriter(detail::SequentialBlobWriter&& blobWriter);

        Result<void> writeIndexFile() const;

        detail::SequentialBlobWriter blobWriter_;

        struct WrittenEntry
        {
            std::string id;
            uint16_t format;
            uint16_t fileSequence;
            uint32_t fileOffset;
            uint32_t length;
        };

        std::vector<WrittenEntry> entries_;
        bool finished_ = false;
    };
}
