//
// kiwakiwaaにより 2026/01/22 に作成されました。
//

#include "monokakido/resource/rsc/rsc_data.hpp"

#include <algorithm>
#include <format>

namespace monokakido
{
    std::expected<RscData, std::string> RscData::load(const fs::path& directoryPath, std::string_view dictId, const uint32_t mapVersion)
    {
        auto filesResult = discoverFiles(directoryPath);
        if (!filesResult)
            return std::unexpected(filesResult.error());

        auto& files = filesResult.value();
        std::ranges::sort(files, {}, &RscResourceFile::sequenceNumber);

        // Calculate and set global offset for each .rsc file
        size_t globalOffset = 0;
        for (size_t i = 0; i < files.size(); ++i)
        {
            if (files[i].sequenceNumber != i)
                return std::unexpected(std::format("Missing resource file with sequence number: {}", i));

            files[i].globalOffset = globalOffset;
            globalOffset += files[i].length;
        }

        std::optional<std::array<uint8_t, 32>> key;
        if (mapVersion == 1 && !dictId.empty())
            key = RscCrypto::deriveKey(dictId);

        return RscData{
            std::move(files),
            key
        };
    }


    std::expected<std::span<const uint8_t>, std::string> RscData::get(const MapRecord& record) const
    {
        // Special case: intraBlock offset of 0xFFFFFFFF means direct data access (no chunk decompression is used)
        if (record.ioffset == 0xFFFFFFFF)
        {
            return readDirectData(record.zOffset);
        }

        // Load the chunk if we haven't loaded it yet or if it's a different chunk
        if (chunkBuffer_.empty() || record.zOffset != currentChunkOffset_)
        {
            if (auto result = loadChunk(record.zOffset); !result)
                return std::unexpected(result.error());
        }

        // Parse the item from the decompressed chunk
        return parseItemFromChunk(record.ioffset);
    }


    RscData::RscData(std::vector<RscResourceFile>&& files, const std::optional<std::array<uint8_t, 32> >& decryptionKey)
        : files_(std::move(files)), decryptionKey_(decryptionKey)
    {
        decompressor_ = std::make_unique<ZlibDecompressor>();
    }


    std::expected<std::span<const uint8_t>, std::string> RscData::parseItemFromChunk(const size_t offset) const
    {
        if (offset >= chunkBuffer_.size())
            return std::unexpected("Invalid offset within chunk");

        const uint8_t* data = chunkBuffer_.data() + offset;
        const size_t remaining = chunkBuffer_.size() - offset;

        // Check minimum header size for old format (4 bytes)
        if (remaining < 4)
            return std::unexpected("Insufficient data for item length");

        // new format has zero as first word
        const uint32_t firstWord = *reinterpret_cast<const uint32_t*>(data);
        const bool isNewFormat = firstWord == 0;

        if (isNewFormat && remaining < 8)
            return std::unexpected("Insufficient data for new format header");

        const size_t headerSize = isNewFormat ? 8 : 4;
        const size_t contentLength = isNewFormat
                                         ? *reinterpret_cast<const uint32_t*>(data + 4)
                                         : firstWord;

        if (remaining < headerSize + contentLength)
            return std::unexpected("Insufficient data for item content");

        return std::span(data + headerSize, contentLength);
    }


    std::expected<void, std::string> RscData::loadChunk(size_t globalOffset) const
    {
        // Find which file contains this offset
        auto fileAndOffset = findFileByOffset(globalOffset);
        if (!fileAndOffset)
            return std::unexpected(fileAndOffset.error());

        auto& [file, localOffset] = *fileAndOffset;

        auto readerResult = BinaryFileReader::open(file.filePath);
        if (!readerResult)
            return std::unexpected(readerResult.error());
        auto& reader = *readerResult;

        // Seek to the local offset
        if (auto seekResult = reader.seek(localOffset); !seekResult)
            return std::unexpected(seekResult.error());

        // Read and process chunk data
        auto dataResult = readAndProcessChunk(reader);
        if (!dataResult)
            return std::unexpected(dataResult.error());

        chunkBuffer_ = std::move(*dataResult);
        currentChunkOffset_ = globalOffset;
        return {};
    }


    std::expected<std::vector<uint8_t>, std::string> RscData::readAndProcessChunk(BinaryFileReader& reader) const
    {
        // Read format marker (first 4 bytes)
        auto versionResult = reader.read<uint32_t>();
        if (!versionResult)
            return std::unexpected(versionResult.error());

        uint32_t& version = versionResult.value();
        std::vector<uint8_t> data;
        if (version == 0)
        {
            auto decryptedData = readAndDecryptData(reader);
            if (!decryptedData)
                return std::unexpected(decryptedData.error());
            data = std::move(*decryptedData);
        }
        else
        {
            // version is the length in the old format
            data.resize(version);
            if (auto result = reader.readBytes(data); !result)
                return std::unexpected(result.error());
        }

        // decompress data if needed
        if (!ZlibDecompressor::isZlibCompressed(data))
            return data;

        if (auto result = decompressor_->decompress(data, data.size()); !result)
            return std::unexpected(result.error());

        return decompressor_->takeBuffer();
    }


    std::expected<std::vector<uint8_t>, std::string> RscData::readAndDecryptData(BinaryFileReader& reader) const
    {
        if (!decryptionKey_)
            return std::unexpected("Encrypted data requires decryption key");

        auto lengthResult = reader.read<uint32_t>();
        if (!lengthResult)
            return std::unexpected(lengthResult.error());

        std::vector<uint8_t> encryptedData(*lengthResult);
        if (auto result = reader.readBytes(encryptedData); !result)
            return std::unexpected(result.error());

        return RscCrypto::decrypt(encryptedData, *decryptionKey_);
    }


    std::expected<std::span<const uint8_t>, std::string> RscData::readDirectData(size_t globalOffset) const
    {
        auto fileAndOffset = findFileByOffset(globalOffset);
        if (!fileAndOffset)
            return std::unexpected(fileAndOffset.error());

        auto& [file, localOffset] = *fileAndOffset;

        auto readerResult = BinaryFileReader::open(file.filePath);
        if (!readerResult)
            return std::unexpected(readerResult.error());
        auto& reader = *readerResult;

        // Seek to the local offset
        if (auto seekResult = reader.seek(localOffset); !seekResult)
            return std::unexpected(seekResult.error());

        // Read the data header to get length
        auto lengthResult = reader.read<uint32_t>();
        if (!lengthResult)
            return std::unexpected(lengthResult.error());

        uint32_t length = lengthResult.value();

        // version is the length in old format
        directDataBuffer_.resize(length);
        if (auto result = reader.readBytes(directDataBuffer_); !result)
            return std::unexpected(result.error());

        return std::span<const uint8_t>(directDataBuffer_);
    }


    std::expected<std::vector<RscResourceFile>, std::string> RscData::discoverFiles(const fs::path& directoryPath)
    {
        std::vector<RscResourceFile> files;

        for (const auto& entry : std::filesystem::directory_iterator(directoryPath))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".rsc")
                continue;

            const auto seqNum = detail::parseSequenceNumber(entry.path().filename(), ".rsc");
            if (!seqNum)
                continue;

            std::error_code ec;
            const auto fileSize = fs::file_size(entry.path(), ec);
            if (ec)
                return std::unexpected(
                    std::format("Failed to get size of '{}': {}", entry.path().string(), ec.message())
                );

            files.push_back(RscResourceFile{
                .sequenceNumber = *seqNum,
                .globalOffset = 0, // Set after discovery
                .length = fileSize,
                .filePath = entry.path()
            });
        }

        if (files.empty())
            return std::unexpected(std::format("No .rsc files found in directory: {}", directoryPath.string()));

        return files;
    }


    std::expected<std::pair<const RscResourceFile&, size_t>, std::string> RscData::findFileByOffset(
        const size_t globalOffset) const
    {
        // Binary search to find which file contains this offset
        auto it = std::ranges::upper_bound(
            files_,
            globalOffset,
            std::less{},
            [](const RscResourceFile& f) { return f.globalOffset; }
        );

        if (it == files_.begin())
            return std::unexpected("Offset is before the first file");

        // Go back to the file that starts at or before this offset
        --it;

        if (const size_t fileEnd = it->globalOffset + it->length; globalOffset >= fileEnd)
            return std::unexpected("Offset is beyond file boundaries");

        size_t localOffset = globalOffset - it->globalOffset;
        return std::pair{std::ref(*it), localOffset};
    }
}
