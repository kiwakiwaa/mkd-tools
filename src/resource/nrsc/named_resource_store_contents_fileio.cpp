//
// kiwakiwaaにより 2026/01/16 に作成されました。
//

#include "MKD/resource/common.hpp"
#include "named_resource_store_contents.hpp"
#include "resource/detail/zlib_stream.hpp"

#include <algorithm>
#include <format>
#include <fstream>

namespace MKD
{
    NamedResourceStoreContents::NamedResourceStoreContents(std::vector<NamedResourceStoreFile>&& files)
        : files_(std::move(files))
    {
    }


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

            files.push_back(NamedResourceStoreFile{
                .sequenceNumber = *seqNum,
                .filePath = entry.path(),
            });
        }

        if (files.empty())
            return std::unexpected(std::format("No .nrsc files found in: {}", directoryPath.string()));

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


    Result<RetainedSpan> NamedResourceStoreContents::readUncompressed(const NamedResourceStoreFile& file, const NamedResourceStoreIndexRecord& record)
    {
        auto bytes = readBytesFromFile(file.filePath, record.offset(), record.len());
        if (!bytes) return std::unexpected(bytes.error());

        auto owned = std::make_shared<const std::vector<uint8_t>>(std::move(*bytes));
        return RetainedSpan{std::move(owned)};
    }


    Result<RetainedSpan> NamedResourceStoreContents::readCompressed(const NamedResourceStoreFile& file, const NamedResourceStoreIndexRecord& record)
    {
        auto bytes = readBytesFromFile(file.filePath, record.offset(), record.len());
        if (!bytes) return std::unexpected(bytes.error());

        const detail::ZlibStream stream;
        auto result = stream.decompress(*bytes, bytes->size());
        if (!result) return std::unexpected(result.error());

        auto owned = std::make_shared<const std::vector<uint8_t>>(stream.takeBuffer());

        return RetainedSpan{std::move(owned)};
    }
}
