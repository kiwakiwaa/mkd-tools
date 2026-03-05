//
// kiwakiwaaにより 2026/01/16 に作成されました。
//

#include "MKD/resource/common.hpp"
#include "nrsc_data.hpp"
#include "../zlib_stream.hpp"

#include <algorithm>
#include <format>
#include <fstream>

namespace MKD
{
    NrscData::NrscData(std::vector<NrscResourceFile>&& files)
        : files_(std::move(files))
    {
    }


    Result<NrscData> NrscData::load(const fs::path& directoryPath)
    {
        auto files = discoverFiles(directoryPath);
        if (!files) return std::unexpected(files.error());
        return NrscData{std::move(*files)};
    }


    Result<std::vector<NrscResourceFile> > NrscData::discoverFiles(const fs::path& directoryPath)
    {
        std::vector<NrscResourceFile> files;

        for (const auto& entry : fs::directory_iterator(directoryPath))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".nrsc")
                continue;

            auto seqNum = detail::parseSequenceNumber(entry.path().filename(), ".nrsc");
            if (!seqNum) continue;

            files.push_back(NrscResourceFile{
                .sequenceNumber = *seqNum,
                .filePath = entry.path(),
            });
        }

        if (files.empty())
            return std::unexpected(std::format("No .nrsc files found in: {}", directoryPath.string()));

        std::ranges::sort(files, {}, &NrscResourceFile::sequenceNumber);
        return files;
    }


    Result<RetainedSpan> NrscData::get(const NrscIndexRecord& record) const
    {
        if (record.fileSequence >= files_.size())
            return std::unexpected(std::format(
                "File index {} out of range (total: {})",
                record.fileSequence, files_.size()));

        auto& file = files_[record.fileSequence];

        switch (record.compressionFormat())
        {
            case CompressionFormat::Uncompressed:
                return readUncompressed(file, record);
            case CompressionFormat::Zlib:
                return readCompressed(file, record);
            default:
                return std::unexpected("Unknown compression format");
        }
    }


    static Result<std::vector<uint8_t> > readBytesFromFile(const fs::path& path, uint64_t offset, size_t length)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            return std::unexpected(std::format(
                "Failed to open '{}'", path.string()));

        stream.seekg(static_cast<std::streamoff>(offset));
        if (!stream)
            return std::unexpected(std::format(
                "Failed to seek to offset {} in '{}'", offset, path.string()));

        std::vector<uint8_t> buffer(length);
        stream.read(reinterpret_cast<char*>(buffer.data()),
                    static_cast<std::streamsize>(length));

        if (!stream)
            return std::unexpected(std::format(
                "Failed to read {} bytes from '{}' at offset {}",
                length, path.string(), offset));

        return buffer;
    }


    Result<RetainedSpan> NrscData::readUncompressed(const NrscResourceFile& file, const NrscIndexRecord& record)
    {
        auto bytes = readBytesFromFile(file.filePath, record.offset(), record.len());
        if (!bytes) return std::unexpected(bytes.error());

        auto owned = std::make_shared<const std::vector<uint8_t>>(std::move(*bytes));
        return RetainedSpan{std::move(owned)};
    }


    Result<RetainedSpan> NrscData::readCompressed(const NrscResourceFile& file, const NrscIndexRecord& record)
    {
        auto bytes = readBytesFromFile(file.filePath, record.offset(), record.len());
        if (!bytes) return std::unexpected(bytes.error());

        const ZlibStream stream;
        auto result = stream.decompress(*bytes, bytes->size());
        if (!result) return std::unexpected(result.error());

        auto owned = std::make_shared<const std::vector<uint8_t>>(stream.takeBuffer());

        return RetainedSpan{std::move(owned)};
    }
}
