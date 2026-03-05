//
// kiwakiwaaにより 2026/02/28 に作成されました。
//

#include "rsc_data.hpp"
#include "../zlib_stream.hpp"

#include <algorithm>
#include <cstring>
#include <format>

namespace MKD
{
    RscData::RscData(std::vector<RscResourceFile>&& files, const std::optional<std::array<uint8_t, 32>>& decryptionKey)
        : files_(std::move(files)), decryptionKey_(decryptionKey), cache_(std::make_unique<ChunkCache>())
    {
    }


    Result<RscData> RscData::load(const fs::path& directoryPath,
                                  std::string_view dictId,
                                  const uint32_t mapVersion)
    {
        auto files = discoverFiles(directoryPath);
        if (!files) return std::unexpected(files.error());

        std::optional<std::array<uint8_t, 32>> key;
        if (mapVersion == 1 && !dictId.empty())
            key = RscCrypto::deriveKey(dictId);

        return RscData{std::move(*files), key};
    }


    Result<std::vector<RscResourceFile>> RscData::discoverFiles(const fs::path& directoryPath)
    {
        std::vector<RscResourceFile> files;

        for (const auto& entry : fs::directory_iterator(directoryPath))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".rsc")
                continue;

            auto seqNum = detail::parseSequenceNumber(entry.path().filename(), ".rsc");
            if (!seqNum) continue;

            auto mapping = MappedFile::open(entry.path());
            if (!mapping) return std::unexpected(mapping.error());

            files.push_back(RscResourceFile{
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

        std::ranges::sort(files, {}, &RscResourceFile::sequenceNumber);

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


    Result<std::span<const uint8_t>> RscData::resolveGlobal(const size_t offset, const size_t length) const
    {
        auto it = std::ranges::upper_bound(
            files_, offset, std::less{},
            [](const RscResourceFile& f) { return f.globalOffset; });

        if (it == files_.begin())
            return std::unexpected("Offset before first file");

        --it;
        return it->mapping.slice(offset - it->globalOffset, length);
    }


    Result<RetainedSpan> RscData::get(const MapRecord& record) const
    {
        if (record.itemOffset == 0xFFFFFFFF)
            return readDirectData(record.chunkGlobalOffset);

        auto chunk = getOrLoadChunk(record.chunkGlobalOffset);
        if (!chunk) return std::unexpected(chunk.error());

        return parseItemFromChunk(std::move(*chunk), record.itemOffset);
    }


    Result<RetainedSpan> RscData::readDirectData(const size_t globalOffset) const
    {
        auto header = resolveGlobal(globalOffset, sizeof(uint32_t));
        if (!header) return std::unexpected(header.error());

        uint32_t length;
        std::memcpy(&length, header->data(), sizeof(uint32_t));

        auto payload = resolveGlobal(globalOffset + sizeof(uint32_t), length);
        if (!payload) return std::unexpected(payload.error());

        return RetainedSpan{*payload};
    }


    Result<std::shared_ptr<const std::vector<uint8_t>>> RscData::getOrLoadChunk(size_t globalOffset) const
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


    Result<std::vector<uint8_t>>RscData::decodeChunkAt(const size_t globalOffset) const
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

        if (!ZlibStream::isZlibCompressed(*raw))
            return std::move(*raw);

        const ZlibStream decompressor;
        if (auto r = decompressor.decompress(*raw, raw->size()); !r)
            return std::unexpected(r.error());

        return decompressor.takeBuffer();
    }


    Result<std::vector<uint8_t>> RscData::readEncryptedRegion(const size_t globalOffset) const
    {
        if (!decryptionKey_)
            return std::unexpected("Missing decryption key");

        auto lenSpan = resolveGlobal(globalOffset, sizeof(uint32_t));
        if (!lenSpan) return std::unexpected(lenSpan.error());

        uint32_t length;
        std::memcpy(&length, lenSpan->data(), sizeof(uint32_t));

        auto encrypted = resolveGlobal(globalOffset + sizeof(uint32_t), length);
        if (!encrypted) return std::unexpected(encrypted.error());

        return RscCrypto::decrypt(*encrypted, *decryptionKey_);
    }


    Result<RetainedSpan> RscData::parseItemFromChunk(
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