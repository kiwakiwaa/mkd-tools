//
// kiwakiwaaにより 2026/02/28 に作成されました。
//

#include "MKD/resource/common.hpp"
#include "named_resource_store_contents.hpp"
#include "resource/detail/zlib_stream.hpp"

#include <algorithm>
#include <format>

namespace MKD
{
    NamedResourceStoreContents::NamedResourceStoreContents(std::vector<NamedResourceStoreFile>&& files)
        : files_(std::move(files)) {}


    Result<NamedResourceStoreContents> NamedResourceStoreContents::load(const fs::path& directoryPath)
    {
        auto files = discoverFiles(directoryPath);
        if (!files) return std::unexpected(files.error());
        return NamedResourceStoreContents{std::move(*files)};
    }


    Result<std::vector<NamedResourceStoreFile>> NamedResourceStoreContents::discoverFiles(const fs::path& directoryPath)
    {
        std::vector<NamedResourceStoreFile> files;

        for (const auto& entry : fs::directory_iterator(directoryPath))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".nrsc")
                continue;

            auto seqNum = detail::parseSequenceNumber(entry.path().filename(), ".nrsc");
            if (!seqNum) continue;

            auto mapping = MappedFile::open(entry.path());
            if (!mapping) return std::unexpected(mapping.error());

            files.push_back(NamedResourceStoreFile{
                .sequenceNumber = *seqNum,
                .filePath = entry.path(),
                .mapping = std::move(*mapping),
            });
        }

        if (files.empty())
            return std::unexpected(std::format(
                "No .nrsc files found in: {}", directoryPath.string()));

        std::ranges::sort(files, {}, &NamedResourceStoreFile::sequenceNumber);
        return files;
    }


    Result<RetainedSpan> NamedResourceStoreContents::get(const NamedResourceStoreIndexRecord& record) const
    {
        if (record.fileSequence >= files_.size())
            return std::unexpected(std::format(
                "File index {} out of range (total: {})",
                record.fileSequence, files_.size()));

        auto& file = files_[record.fileSequence];

        if (record.isCompressed())
            return readCompressed(file, record);

        return readUncompressed(file, record);
    }


    Result<RetainedSpan> NamedResourceStoreContents::readUncompressed(const NamedResourceStoreFile& file, const NamedResourceStoreIndexRecord& record)
    {
        auto span = file.mapping.slice(record.offset(), record.len());
        if (!span) return std::unexpected(span.error());

        // span directly into mmap
        return RetainedSpan{*span};
    }


    Result<RetainedSpan> NamedResourceStoreContents::readCompressed(const NamedResourceStoreFile& file, const NamedResourceStoreIndexRecord& record)
    {
        auto compressed = file.mapping.slice(record.offset(), record.len());
        if (!compressed) return std::unexpected(compressed.error());

        const detail::ZlibStream decompressor;
        if (auto result = decompressor.decompress(*compressed, record.len()); !result)
            return std::unexpected(result.error());

        auto buffer = std::make_shared<const std::vector<uint8_t>>(decompressor.takeBuffer());

        return RetainedSpan{std::move(buffer)};
    }
}