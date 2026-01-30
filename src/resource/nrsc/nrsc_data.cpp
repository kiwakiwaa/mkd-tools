//
// kiwakiwaaにより 2026/01/16 に作成されました。
//

#include "monokakido/resource/nrsc/nrsc_data.hpp"
#include "monokakido/resource/common.hpp"

#include <algorithm>
#include <format>

namespace monokakido::resource
{
    std::expected<NrscData, std::string> NrscData::load(const fs::path& directoryPath)
    {
        std::vector<ResourceFile> files;

        for (const auto& entry : fs::directory_iterator(directoryPath))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".nrsc")
                continue;

            const auto seqNum = detail::parseSequenceNumber(entry.path().filename(), ".nrsc");
            if (!seqNum)
                continue;

            files.push_back(ResourceFile{
                .sequenceNumber = *seqNum,
                .filePath = entry.path()
            });
        }

        if (files.empty())
            return std::unexpected(std::format("No .nrsc files found in directory: {}", directoryPath.string()));

        std::ranges::sort(files, {}, &ResourceFile::sequenceNumber);

        return NrscData{std::move(files)};
    }


    std::expected<std::span<const uint8_t>, std::string> NrscData::get(const NrscIndexRecord& record) const
    {
        if (record.fileSequence >= files_.size())
            return std::unexpected(std::format("File index '{}' out of range, total files: {}", record.fileSequence,
                                               files_.size()));

        auto& file = files_[record.fileSequence];

        // Read raw bytes into readBuffer_
        if (auto readResult = readFromFile(file, record); !readResult)
            return std::unexpected(readResult.error());

        // decompress if needed
        return decompressData(record);
    }


    NrscData::NrscData(std::vector<ResourceFile>&& files)
        : files_(std::move(files)), decompressor_(std::make_unique<ZlibDecompressor>())
    {
    }


    std::expected<void, std::string> NrscData::readFromFile(const ResourceFile& file,
                                                            const NrscIndexRecord& record) const
    {
        const uint64_t fileOffset = record.offset();

        std::ifstream stream(file.filePath, std::ios::binary);
        if (!stream)
            return std::unexpected(std::format("Failed to open file '{}'", file.filePath.string()));

        stream.seekg(static_cast<std::streamoff>(fileOffset));
        if (!stream)
        {
            return std::unexpected(std::format("Failed to seek to offset '{}' from '{}'",
                                               fileOffset, file.filePath.string()));
        }

        if (readBuffer_.size() < record.len())
            readBuffer_.resize(record.len());

        stream.read(reinterpret_cast<char*>(readBuffer_.data()),
                    static_cast<std::streamsize>(record.len()));
        if (!stream)
        {
            return std::unexpected(std::format(
                "Failed to read {} bytes from '{}' at offset {}",
                record.len(), file.filePath.string(), fileOffset));
        }

        return {};
    }


    std::expected<std::span<const uint8_t>, std::string> NrscData::decompressData(const NrscIndexRecord& record) const
    {
        const auto format = record.compressionFormat();
        const std::span<const uint8_t> compressed(readBuffer_.data(), record.len());

        switch (format)
        {
            case CompressionFormat::Uncompressed:
                return compressed;

            case CompressionFormat::Zlib:
                return decompressor_->decompress(compressed, record.len());

            default:
                return std::unexpected("Unknown compression format");
        }
    }
}
