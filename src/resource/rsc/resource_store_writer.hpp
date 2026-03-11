//
// kiwakiwaaにより 2026/03/06 に作成されました。
//

#pragma once

#include "MKD/result.hpp"
#include "resource_store_index.hpp"
#include "resource_store_contents.hpp"
#include "resource/detail/sequential_blob_writer.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace MKD
{
    class ResourceStoreWriter
    {
    public:
        struct Options
        {
            ResourceType type;
            size_t maxFileSize = 3 * 1024 * 1024;
            size_t chunkSize = 32;
            int compressionLevel = 9;
            bool useEncryption = false;
            std::string dictId {}; // required when using encryption
        };

        static Result<ResourceStoreWriter> createIndexed(const fs::path& directoryPath, const Options& options);


        static Result<ResourceStoreWriter> createSequential(const fs::path& directoryPath, const Options& options);


        /**
         * Add an item with explicit ID, uses .idx
         * @param itemId id of the data item
         * @param data data buffer
         * @return Void or error string if failure
         *
         * @note Not callable when using sequential writer
         */
        Result<void> add(uint32_t itemId, std::vector<uint8_t> data);

        /**
         * Add sequential data to, only uses .map
         * @param data span of the data
         * @return Void or error string if failure
         *
         * @note Not callable when using indexed writer
         */
        Result<void> addData(std::span<const uint8_t> data);

        Result<void> finalize();

        [[nodiscard]] size_t size() const noexcept;
        [[nodiscard]] bool empty() const noexcept;

    private:
        enum class Mode { Indexed, Sequential };

        explicit ResourceStoreWriter(Mode mode, const Options& options, detail::SequentialBlobWriter&& blobWriter);

        static Result<ResourceStoreWriter> createImpl(Mode mode, const fs::path& directoryPath, const Options& options);

        struct PendingItem
        {
            uint32_t itemId;
            std::vector<uint8_t> data;
        };

        struct SerializedChunk
        {
            std::vector<uint8_t> data;
            std::vector<uint32_t> itemOffsets;
        };

        /**
         * Concatenate item headers + data into a raw chunk buffer
         * Encrpted format: [4-byte zero][4-byte length][data]
         * Old format: [4-byte length][data]
         * @param items items to serialize
         * @return Serialized chunk of items ready to be compressed
         */
        SerializedChunk serializeItems(std::span<const PendingItem> items) const;

        struct ChunkResult
        {
            std::vector<uint8_t> compressed;
            std::vector<uint32_t> itemOffsets; // offset of each item within decompressed chunk
        };

        Result<void> flushChunk();

        Result<void> writeIndexFiles() const;

        Mode mode_;
        Options options_;
        detail::SequentialBlobWriter blobWriter_;

        std::vector<PendingItem> pendingItems_;
        std::vector<ChunkResult> pendingChunks_;
        size_t pendingBytes_ = 0;

        std::vector<ResourceStoreIndexRecord> idxRecords_;
        std::vector<ResourceStoreMapRecord> mapRecords_;
        std::optional<std::array<uint8_t, 32>> encryptionKey_;
        bool finished_ = false;
    };
}
