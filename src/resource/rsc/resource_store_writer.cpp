//
// kiwakiwaaにより 2026/03/06 に作成されました。
//

#include "resource_store_writer.hpp"
#include "resource/detail/binary_buffer.hpp"
#include "resource/detail/zlib_stream.hpp"

#include <algorithm>

namespace MKD
{
    Result<ResourceStoreWriter> ResourceStoreWriter::createIndexed(const fs::path& directoryPath,
                                                                   const Options& options)
    {
        return createImpl(Mode::Indexed, directoryPath, options);
    }


    Result<ResourceStoreWriter> ResourceStoreWriter::createSequential(const fs::path& directoryPath,
                                                                      const Options& options)
    {
        return createImpl(Mode::Sequential, directoryPath, options);
    }


    ResourceStoreWriter::ResourceStoreWriter(const Mode mode, const Options& options,
                                             detail::SequentialBlobWriter&& blobWriter)
        : mode_(mode), options_(options), blobWriter_(std::move(blobWriter))
    {
        if (options_.useEncryption && !options_.dictId.empty())
            encryptionKey_ = ResourceStoreCrypto::deriveKey(options_.dictId);
    }


    Result<ResourceStoreWriter> ResourceStoreWriter::createImpl(const Mode mode, const fs::path& directoryPath,
                                                                const Options& options)
    {
        auto blobWriter = detail::SequentialBlobWriter::create(
            directoryPath, ResourceType::Contents, options.maxFileSize);

        if (!blobWriter)
            return std::unexpected(blobWriter.error());

        return ResourceStoreWriter(mode, options, std::move(*blobWriter));
    }


    Result<void> ResourceStoreWriter::add(const uint32_t itemId, std::vector<uint8_t> data)
    {
        if (finished_)
            return std::unexpected("Cannot add items after finalize()");
        if (mode_ != Mode::Indexed)
            return std::unexpected("add(itemId, data) is only valid for indexed writers, use addData() instead");

        pendingItems_.push_back({.itemId = itemId, .data = std::move(data)});

        if (pendingItems_.size() >= options_.chunkSize)
            return flushChunk();

        return {};
    }


    Result<void> ResourceStoreWriter::addData(const std::span<const uint8_t> data)
    {
        if (finished_)
            return std::unexpected("Cannot add items after finalize()");
        if (mode_ != Mode::Sequential)
            return std::unexpected("addData() is only valid for sequential writers, use add() instead");

        auto location = blobWriter_.write(data);
        if (!location)
            return std::unexpected(location.error());

        mapRecords_.push_back({
            .chunkGlobalOffset = static_cast<uint32_t>(location->globalOffset),
            .itemOffset = 0xFFFFFFFF
        });

        return {};
    }


    Result<void> ResourceStoreWriter::finalize()
    {
        if (finished_)
            return std::unexpected("finalize() already called");

        if (!pendingItems_.empty())
            if (auto result = flushChunk(); !result)
                return std::unexpected(result.error());

        if (auto blobResult = blobWriter_.finalize(); !blobResult)
            return std::unexpected(blobResult.error());

        if (auto indexResult = writeIndexFiles(); !indexResult)
            return std::unexpected(indexResult.error());

        finished_ = true;
        return {};
    }


    size_t ResourceStoreWriter::size() const noexcept
    {
        return !idxRecords_.empty()
                   ? idxRecords_.size() + pendingItems_.size()
                   : mapRecords_.size() + pendingItems_.size();
    }


    bool ResourceStoreWriter::empty() const noexcept
    {
        return idxRecords_.empty() && mapRecords_.empty() && pendingItems_.empty();
    }


    ResourceStoreWriter::SerializedChunk ResourceStoreWriter::serializeItems(std::span<const PendingItem> items) const
    {
        const bool newFormat = encryptionKey_.has_value();
        const size_t headerSize = newFormat ? 8 : 4;

        // calculate total size and record item offsets
        size_t totalSize = 0;
        std::vector<uint32_t> offsets;
        offsets.reserve(items.size());

        for (const auto& [itemId, data] : items)
        {
            offsets.push_back(static_cast<uint32_t>(totalSize));
            totalSize += headerSize + data.size();
        }

        // write item headers and data
        detail::BinaryBuffer buffer(totalSize);
        for (const auto& [itemId, data] : items)
        {
            if (newFormat)
                buffer.writePadding(sizeof(uint32_t));

            buffer.writeLE(static_cast<uint32_t>(data.size()));
            buffer.writeBytes(data);
        }

        return {std::move(buffer.take()), std::move(offsets)};
    }


    Result<void> ResourceStoreWriter::flushChunk()
    {
        const auto [serialized, itemOffsets] = serializeItems(pendingItems_);

        const detail::ZlibStream zlib;
        auto compressedSpan = zlib.compress(serialized, options_.compressionLevel);
        if (!compressedSpan)
            return std::unexpected(compressedSpan.error());

        detail::BinaryBuffer buffer;
        if (encryptionKey_)
        {
            auto encrypted = ResourceStoreCrypto::encrypt(*compressedSpan, *encryptionKey_);
            buffer.writeLE(uint32_t{0}); // new-format marker
            buffer.writeLE(static_cast<uint32_t>(encrypted.size()));
            buffer.writeBytes(encrypted);
        }
        else
        {
            buffer.writeLE(static_cast<uint32_t>(compressedSpan->size()));
            buffer.writeBytes(*compressedSpan);
        }

        auto location = blobWriter_.write(buffer.data());
        if (!location)
            return std::unexpected(location.error());

        // record index entries
        const auto chunkOffset = static_cast<uint32_t>(location->globalOffset);
        for (size_t i = 0; i < pendingItems_.size(); ++i)
        {
            const auto mapIndex = static_cast<uint32_t>(mapRecords_.size());
            mapRecords_.push_back({
                .chunkGlobalOffset = chunkOffset,
                .itemOffset = itemOffsets[i],
            });

            idxRecords_.push_back({
                .itemId = pendingItems_[i].itemId,
                .mapIdx = mapIndex,
            });
        }

        pendingItems_.clear();
        return {};
    }


    Result<void> ResourceStoreWriter::writeIndexFiles() const
    {
        const auto directory = blobWriter_.directory();

        // Write contents.map
        {
            const auto mapPath = directory / "contents.map";
            std::ofstream file(mapPath, std::ios::binary | std::ios::trunc);
            if (!file)
                return std::unexpected(std::format("Failed to open: {}", mapPath.string()));

            ResourceStoreMapHeader header{
                .version = encryptionKey_ ? 0x01u : 0x00u,
                .recordCount = static_cast<uint32_t>(idxRecords_.size()),
            };

            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            file.write(reinterpret_cast<const char*>(mapRecords_.data()),
                       static_cast<std::streamsize>(mapRecords_.size() * sizeof(ResourceStoreMapRecord)));

            if (!file)
                return std::unexpected(std::format("Failed to write: {}", mapPath.string()));
        }

        // write contents.idx
        if (mode_ == Mode::Indexed)
        {
            const auto idxPath = directory / "contents.idx";
            std::ofstream file(idxPath, std::ios::binary | std::ios::trunc);
            if (!file)
                return std::unexpected(std::format("Failed to open: {}", idxPath.string()));

            // Sort idx records by itemId for binary search at read time
            auto sortedIdx = idxRecords_;
            std::ranges::sort(sortedIdx, {}, &ResourceStoreIndexRecord::itemId);

            ResourceStoreIndexHeader header{
                .length = static_cast<uint32_t>(sortedIdx.size()),
                .padding = 0
            };

            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            file.write(reinterpret_cast<const char*>(sortedIdx.data()),
                       static_cast<std::streamsize>(sortedIdx.size() * sizeof(ResourceStoreIndexRecord)));

            if (!file)
                return std::unexpected(std::format("Failed to write: {}", idxPath.string()));
        }

        return {};
    }
}
