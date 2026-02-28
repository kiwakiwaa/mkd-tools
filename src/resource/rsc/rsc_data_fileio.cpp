//
// kiwakiwaaにより 2026/01/22 に作成されました。
//

#include "MKD/resource/rsc/rsc_data.hpp"

#include <algorithm>
#include <format>

namespace MKD
{
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


    RscData::RscData(std::vector<RscResourceFile>&& files, const std::optional<std::array<uint8_t, 32>>& decryptionKey)
    : files_(std::move(files)), decryptionKey_(decryptionKey)
    {
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

            std::error_code ec;
            const auto fileSize = fs::file_size(entry.path(), ec);
            if (ec)
                return std::unexpected(std::format(
                    "Failed to get size of '{}': {}", entry.path().string(), ec.message()));

            files.push_back(RscResourceFile{
                .sequenceNumber = *seqNum,
                .globalOffset = 0,
                .length = fileSize,
                .filePath = entry.path(),
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


    Result<std::pair<const RscResourceFile&, size_t>> RscData::findFileByOffset(const size_t globalOffset) const
    {
        auto it = std::ranges::upper_bound(
            files_, globalOffset, std::less{},
            [](const RscResourceFile& f) { return f.globalOffset; });

        if (it == files_.begin())
            return std::unexpected("Offset before first file");

        --it;

        if (globalOffset >= it->globalOffset + it->length)
            return std::unexpected("Offset beyond file boundaries");

        return std::pair{std::cref(*it), globalOffset - it->globalOffset};
    }


    Result<BinaryFileReader> RscData::openReaderAt(const size_t globalOffset) const
    {
        return findFileByOffset(globalOffset)
            .and_then([](auto location) -> Result<BinaryFileReader> {
                auto& [file, localOffset] = location;
                return BinaryFileReader::open(file.get().filePath)
                    .and_then([&](auto reader) -> Result<BinaryFileReader> {
                        auto s = reader.seek(localOffset);
                        if (!s) return std::unexpected(s.error());
                        return std::move(reader);
                    });
            });
    }


    Result<OwnedSpan> RscData::get(const MapRecord& record) const
    {
        if (record.ioffset == 0xFFFFFFFF)
            return readDirectData(record.chunkGlobalOffset);

        auto chunk = loadChunk(record.chunkGlobalOffset);
        if (!chunk) return std::unexpected(chunk.error());

        return parseItemFromChunk(std::move(*chunk), record.ioffset);
    }


    Result<OwnedSpan> RscData::readDirectData(size_t globalOffset) const
    {
        auto reader = openReaderAt(globalOffset);
        if (!reader) return std::unexpected(reader.error());

        auto seq = reader->sequence();
        const auto length = seq.readValue<uint32_t>();
        auto bytes = seq.readBytes(length);

        if (!seq) return std::unexpected(seq.error());

        auto owned = std::make_shared<const std::vector<uint8_t>>(std::move(bytes));
        return OwnedSpan{std::move(owned)};
    }


    Result<std::shared_ptr<const std::vector<uint8_t>>> RscData::loadChunk(size_t globalOffset) const
    {
        if (currentChunk_ && currentChunkOffset_ == globalOffset)
            return currentChunk_;

        auto reader = openReaderAt(globalOffset);
        if (!reader) return std::unexpected(reader.error());

        auto data = readAndProcessChunk(*reader);
        if (!data) return std::unexpected(data.error());

        currentChunk_ = std::make_shared<const std::vector<uint8_t>>(std::move(*data));
        currentChunkOffset_ = globalOffset;
        return currentChunk_;
    }


    Result<std::vector<uint8_t>> RscData::readAndProcessChunk(BinaryFileReader& reader) const
    {
        auto marker = reader.read<uint32_t>();
        if (!marker) return std::unexpected(marker.error());

        auto data = (*marker == 0)
            ? readAndDecryptData(reader)
            : reader.readBytes(*marker).transform([](auto bytes) { return bytes; });

        if (!data) return std::unexpected(data.error());

        if (!ZlibDecompressor::isZlibCompressed(*data))
            return std::move(*data);

        ZlibDecompressor decompressor;
        if (auto r = decompressor.decompress(*data, data->size()); !r)
            return std::unexpected(r.error());

        return decompressor.takeBuffer();
    }


    Result<std::vector<uint8_t>> RscData::readAndDecryptData(BinaryFileReader& reader) const
    {
        if (!decryptionKey_)
            return std::unexpected("Missing decryption key");

        auto seq = reader.sequence();
        const auto length = seq.readValue<uint32_t>();
        auto encrypted = seq.readBytes(length);

        if (!seq) return std::unexpected(seq.error());

        return RscCrypto::decrypt(encrypted, *decryptionKey_);
    }


    // Shared between both backends
    Result<OwnedSpan> RscData::parseItemFromChunk(
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

        return OwnedSpan{
            std::move(chunk),
            std::span(data + headerSize, contentLength)
        };
    }
}