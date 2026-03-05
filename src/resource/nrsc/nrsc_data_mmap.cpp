//
// kiwakiwaaにより 2026/02/28 に作成されました。
//

#include "MKD/resource/common.hpp"
#include "nrsc_data.hpp"
#include "../zlib_stream.hpp"

#include <algorithm>
#include <format>

namespace MKD
{
    NrscData::NrscData(std::vector<NrscResourceFile>&& files)
        : files_(std::move(files)) {}


    Result<NrscData> NrscData::load(const fs::path& directoryPath)
    {
        auto files = discoverFiles(directoryPath);
        if (!files) return std::unexpected(files.error());
        return NrscData{std::move(*files)};
    }


    Result<std::vector<NrscResourceFile>> NrscData::discoverFiles(const fs::path& directoryPath)
    {
        std::vector<NrscResourceFile> files;

        for (const auto& entry : fs::directory_iterator(directoryPath))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".nrsc")
                continue;

            auto seqNum = detail::parseSequenceNumber(entry.path().filename(), ".nrsc");
            if (!seqNum) continue;

            auto mapping = MappedFile::open(entry.path());
            if (!mapping) return std::unexpected(mapping.error());

            files.push_back(NrscResourceFile{
                .sequenceNumber = *seqNum,
                .filePath = entry.path(),
                .mapping = std::move(*mapping),
            });
        }

        if (files.empty())
            return std::unexpected(std::format(
                "No .nrsc files found in: {}", directoryPath.string()));

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


    Result<RetainedSpan> NrscData::readUncompressed(const NrscResourceFile& file, const NrscIndexRecord& record)
    {
        auto span = file.mapping.slice(record.offset(), record.len());
        if (!span) return std::unexpected(span.error());

        // span directly into mmap
        return RetainedSpan{*span};
    }


    Result<RetainedSpan> NrscData::readCompressed(const NrscResourceFile& file, const NrscIndexRecord& record)
    {
        auto compressed = file.mapping.slice(record.offset(), record.len());
        if (!compressed) return std::unexpected(compressed.error());

        const ZlibStream decompressor;
        if (auto result = decompressor.decompress(*compressed, record.len()); !result)
            return std::unexpected(result.error());

        auto buffer = std::make_shared<const std::vector<uint8_t>>(decompressor.takeBuffer());

        return RetainedSpan{std::move(buffer)};
    }
}