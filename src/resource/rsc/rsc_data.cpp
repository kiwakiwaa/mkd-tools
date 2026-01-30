//
// kiwakiwaaにより 2026/01/22 に作成されました。
//

#include "monokakido/resource/rsc/rsc_data.hpp"
#include "monokakido/core/platform/fs.hpp"

#include <algorithm>
#include <format>

namespace monokakido::resource
{
    std::expected<RscData, std::string> RscData::load(const fs::path& directoryPath, std::string_view dictId)
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

        // TODO: get optional decryption key here for the given dictId
        return RscData{std::move(files), std::nullopt};
    }


    std::expected<std::span<const uint8_t>, std::string> RscData::get(const MapRecord& record)
    {
        // Load the chunk if we haven't loaded it yet or if it's a different chunk
        if (chunkBuffer_.empty() && record.zOffset != currentChunkOffset_)
        {
            if (auto result = loadChunk(record.zOffset); !result)
                return std::unexpected(result.error());
        }

        // Parse the item from the decompressed chunk
        return parseItemFromChunk(record.ioffset);
    }


    RscData::RscData(std::vector<RscResourceFile>&& files, std::optional<std::array<uint8_t, 32> >&& decryptionKey)
        : files_(std::move(files)), decryptionKey_(decryptionKey)
    {
        decompressor_ = std::make_unique<ZlibDecompressor>();
    }


    std::expected<void, std::string> RscData::loadChunk(size_t globalOffset)
    {
        // Find which file contains this offset
        auto fileAndOffset = findFileByOffset(globalOffset);
        if (!fileAndOffset)
            return std::unexpected(fileAndOffset.error());

        auto& [file, localOffset] = *fileAndOffset;

        auto readerResult = platform::fs::BinaryFileReader::open(file.filePath);
        if (!readerResult)
            return std::unexpected(readerResult.error());
        auto& reader = *readerResult;

        // Seek to the local offset
        if (auto seekResult = reader.seek(localOffset); !seekResult)
            return std::unexpected(seekResult.error());

        // Read version/format marker (first 4 bytes)
        auto versionResult = reader.read<uint32_t>();
        if (!versionResult)
            return std::unexpected(versionResult.error());
        uint32_t& version = versionResult.value();

        std::vector<uint8_t> compressedData;

        if (version == 0)
        {
            // New encrypted format
            if (!decryptionKey_)
                return std::unexpected("Encrypted data requires decryption key");
            // implement decryption module later
        }
        else
        {
            // Old format: version bytes are actually the length
            const size_t compressedLength = version;
            compressedData.resize(compressedLength);

            if (auto result = reader.readBytes(compressedData); !result)
                return std::unexpected(result.error());
        }

        // Decompress into chunkBuffer_
        chunkBuffer_.clear();
        if (ZlibDecompressor::isZlibCompressed(compressedData))
        {
            if (auto result = decompressor_->decompress(compressedData, compressedData.size()); !result)
                return std::unexpected(result.error());

            chunkBuffer_ = std::move(decompressor_->takeBuffer());
        }
        else
        {
            // Not compressed, use directly
            chunkBuffer_ = std::move(compressedData);
        }

        currentChunkOffset_ = globalOffset;
        return {};
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


    std::expected<std::span<const uint8_t>, std::string> RscData::parseItemFromChunk(const size_t offset) const
    {
        if (offset >= chunkBuffer_.size())
            return std::unexpected("Invalid offset within chunk");

        const uint8_t* data = chunkBuffer_.data() + offset;
        const size_t remaining = chunkBuffer_.size() - offset;

        if (remaining < 4)
            return std::unexpected("Insufficient data for item length");

        // Detect format by checking for XML header
        const bool isOldFormat = (remaining >= 5 && std::memcmp(data, "<?xml", 5) == 0) ||
                                 (remaining >= 8 && std::memcmp(data + 4, "<?xm", 4) == 0);

        size_t contentLength;
        size_t contentStart;

        if (isOldFormat)
        {
            // Old format: 4-byte length, then content
            contentLength = *reinterpret_cast<const uint32_t*>(data);
            contentStart = 4;
        }
        else
        {
            // New format: 8-byte header
            if (remaining < 8)
                return std::unexpected("Insufficient data for new format header");

            contentLength = *reinterpret_cast<const uint32_t*>(data + 4);
            contentStart = 8;
        }

        if (remaining < contentStart + contentLength)
            return std::unexpected("Insufficient data for item content");

        return std::span(data + contentStart, contentLength);
    }


    std::expected<std::pair<RscResourceFile&, size_t>, std::string> RscData::findFileByOffset(const size_t globalOffset)
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
