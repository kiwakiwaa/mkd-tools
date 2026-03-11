//
// kiwakiwaaにより 2026/02/28 に作成されました。
//

#include "resource_store_contents.hpp"
#include "resource/detail/zlib_stream.hpp"

#include <algorithm>
#include <cstring>
#include <format>

namespace MKD
{
    ResourceStoreContents::ResourceStoreContents(std::vector<ResourceStoreContentsFile>&& files, const std::optional<std::array<uint8_t, 32>>& decryptionKey)
        : files_(std::move(files)), decryptionKey_(decryptionKey), cache_(std::make_unique<ChunkCache>())
    {
    }


    Result<ResourceStoreContents> ResourceStoreContents::load(const fs::path& directoryPath,
                                  std::string_view dictId,
                                  const uint32_t mapVersion)
    {
        auto files = discoverFiles(directoryPath);
        if (!files) return std::unexpected(files.error());

        std::optional<std::array<uint8_t, 32>> key;
        if (mapVersion == 1 && !dictId.empty())
            key = ResourceStoreCrypto::deriveKey(dictId);

        return ResourceStoreContents{std::move(*files), key};
    }


    Result<std::vector<ResourceStoreContentsFile>> ResourceStoreContents::discoverFiles(const fs::path& directoryPath)
    {
        std::vector<ResourceStoreContentsFile> files;

        for (const auto& entry : fs::directory_iterator(directoryPath))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".rsc")
                continue;

            auto seqNum = detail::parseSequenceNumber(entry.path().filename(), ".rsc");
            if (!seqNum) continue;

            auto mapping = MappedFile::open(entry.path());
            if (!mapping) return std::unexpected(mapping.error());

            files.push_back(ResourceStoreContentsFile{
                .sequenceNumber = *seqNum,
                .globalOffset = 0,
                .length = mapping->size(),
                .filePath = entry.path(),
                .mapping = std::move(*mapping),
            });
        }

        if (files.empty())
            return std::unexpected(std::format(
                "No .rsc files found in: {}", directoryPath.string()));

        std::ranges::sort(files, {}, &ResourceStoreContentsFile::sequenceNumber);

        size_t offset = 0;
        for (size_t i = 0; i < files.size(); ++i)
        {
            if (files[i].sequenceNumber != i)
                return std::unexpected(std::format(
                    "Missing .rsc file with sequence number: {}", i));
            files[i].globalOffset = offset;
            offset += files[i].length;
        }

        return files;
    }


    Result<std::span<const uint8_t>> ResourceStoreContents::resolveGlobal(const size_t offset, const size_t length) const
    {
        auto it = std::ranges::upper_bound(
            files_, offset, std::less{},
            [](const ResourceStoreContentsFile& f) { return f.globalOffset; });

        if (it == files_.begin())
            return std::unexpected("Offset before first file");

        --it;
        return it->mapping.slice(offset - it->globalOffset, length);
    }


    Result<RetainedSpan> ResourceStoreContents::get(const ResourceStoreMapRecord& record) const
    {
        if (record.itemOffset == 0xFFFFFFFF)
            return readDirectData(record.chunkGlobalOffset);

        auto chunk = getOrLoadChunk(record.chunkGlobalOffset);
        if (!chunk) return std::unexpected(chunk.error());

        return parseItemFromChunk(std::move(*chunk), record.itemOffset);
    }


    Result<RetainedSpan> ResourceStoreContents::readDirectData(const size_t globalOffset) const
    {
        auto header = resolveGlobal(globalOffset, sizeof(uint32_t));
        if (!header) return std::unexpected(header.error());

        uint32_t length;
        std::memcpy(&length, header->data(), sizeof(uint32_t));

        auto payload = resolveGlobal(globalOffset + sizeof(uint32_t), length);
        if (!payload) return std::unexpected(payload.error());

        return RetainedSpan{*payload};
    }


    Result<std::shared_ptr<const std::vector<uint8_t>>> ResourceStoreContents::getOrLoadChunk(size_t globalOffset) const
    {
        {
            std::lock_guard lock(cache_->mutex);
            if (const auto it = cache_->entries.find(globalOffset); it != cache_->entries.end())
                return it->second;
        }

        auto decoded = decodeChunkAt(globalOffset);
        if (!decoded) return std::unexpected(decoded.error());

        auto shared = std::make_shared<const std::vector<uint8_t>>(std::move(*decoded));

        {
            std::lock_guard lock(cache_->mutex);
            if (cache_->entries.size() >= MAX_CACHED_CHUNKS)
                cache_->entries.clear();

            auto [it, _] = cache_->entries.emplace(globalOffset, shared);
            return it->second;
        }
    }


    Result<std::vector<uint8_t>>ResourceStoreContents::decodeChunkAt(const size_t globalOffset) const
    {
        auto markerSpan = resolveGlobal(globalOffset, sizeof(uint32_t));
        if (!markerSpan) return std::unexpected(markerSpan.error());

        uint32_t marker;
        std::memcpy(&marker, markerSpan->data(), sizeof(uint32_t));
        const size_t dataStart = globalOffset + sizeof(uint32_t);

        auto raw = marker == 0
            ? readEncryptedRegion(dataStart)
            : resolveGlobal(dataStart, marker).transform([](auto span) {
                  return std::vector<uint8_t>(span.begin(), span.end());
              });

        if (!raw) return std::unexpected(raw.error());

        if (!detail::ZlibStream::isZlibCompressed(*raw))
            return std::move(*raw);

        const detail::ZlibStream decompressor;
        if (auto r = decompressor.decompress(*raw, raw->size()); !r)
            return std::unexpected(r.error());

        return decompressor.takeBuffer();
    }


    Result<std::vector<uint8_t>> ResourceStoreContents::readEncryptedRegion(const size_t globalOffset) const
    {
        if (!decryptionKey_)
            return std::unexpected("Missing decryption key");

        auto lenSpan = resolveGlobal(globalOffset, sizeof(uint32_t));
        if (!lenSpan) return std::unexpected(lenSpan.error());

        uint32_t length;
        std::memcpy(&length, lenSpan->data(), sizeof(uint32_t));

        auto encrypted = resolveGlobal(globalOffset + sizeof(uint32_t), length);
        if (!encrypted) return std::unexpected(encrypted.error());

        return ResourceStoreCrypto::decrypt(*encrypted, *decryptionKey_);
    }


    Result<RetainedSpan> ResourceStoreContents::parseItemFromChunk(
        std::shared_ptr<const std::vector<uint8_t>> chunk,
        const size_t offset)
    {
        if (offset + 4 > chunk->size())
            return std::unexpected("Invalid offset within chunk");

        const uint8_t* data = chunk->data() + offset;
        const size_t remaining = chunk->size() - offset;

        uint32_t firstWord;
        std::memcpy(&firstWord, data, sizeof(uint32_t));

        const bool isNewFormat = (firstWord == 0);
        const size_t headerSize = isNewFormat ? 8 : 4;

        if (remaining < headerSize)
            return std::unexpected("Insufficient data for item header");

        uint32_t contentLength = firstWord;
        if (isNewFormat)
            std::memcpy(&contentLength, data + 4, sizeof(uint32_t));

        if (remaining < headerSize + contentLength)
            return std::unexpected("Insufficient data for item content");

        return RetainedSpan{
            std::move(chunk),
            std::span(data + headerSize, contentLength)
        };
    }
}