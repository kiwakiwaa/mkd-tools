//
// kiwakiwaaにより 2026/03/07 に作成されました。
//

#include "named_resource_store_writer.hpp"
#include "named_resource_store_index.hpp"
#include "resource/detail/zlib_stream.hpp"

#include <algorithm>
#include <numeric>

namespace MKD
{
    Result<NamedResourceStoreWriter> NamedResourceStoreWriter::create(const fs::path& directoryPath, const Options& options)
    {
        if (options.type != ResourceType::Audio && options.type != ResourceType::Graphics)
            return std::unexpected(std::format("Resource type '{}' is not supported by nrsc", resourceTypeName(options.type)));

        auto blobWriter = detail::SequentialBlobWriter::create(
            directoryPath, options.type, options.maxFileSize);

        if (!blobWriter)
            return std::unexpected(blobWriter.error());

        return NamedResourceStoreWriter(std::move(*blobWriter));
    }


    NamedResourceStoreWriter::NamedResourceStoreWriter(detail::SequentialBlobWriter&& blobWriter)
        : blobWriter_(std::move(blobWriter))
    {
    }


    Result<void> NamedResourceStoreWriter::add(std::string_view id, std::vector<uint8_t> data, const int compressionLevel)
    {
        if (finished_)
            return std::unexpected("Cannot add entries after finalize()");

        if (id.empty())
            return std::unexpected("Entry ID must not be empty");

        const bool compress = compressionLevel > 0;
        std::span<const uint8_t> dataToWrite;
        std::vector<uint8_t> compressedData;

        if (compress)
        {
            const detail::ZlibStream zlib;
            auto compressed = zlib.compress(data, compressionLevel);
            if (!compressed)
                return std::unexpected(compressed.error());

            compressedData.assign(compressed->begin(), compressed->end());
            dataToWrite = compressedData;
        }
        else
        {
            dataToWrite = data;
        }

        // Capture per-file position before writing
        const auto fileSeq = static_cast<uint16_t>(blobWriter_.fileSequence());
        const auto fileOffset = static_cast<uint32_t>(blobWriter_.currentFileOffset());

        if (auto location = blobWriter_.write(dataToWrite); !location)
            return std::unexpected(location.error());

        entries_.push_back({
            .id = std::move(std::string(id)),
            .format = static_cast<uint16_t>(compress ? 1u : 0u),
            .fileSequence = fileSeq,
            .fileOffset = fileOffset,
            .length = static_cast<uint32_t>(dataToWrite.size()),
        });

        return {};
    }


    Result<void> NamedResourceStoreWriter::finalize()
    {
        if (finished_)
            return std::unexpected("finalize() already called");

        if (auto result = blobWriter_.finalize(); !result)
            return std::unexpected(result.error());

        if (auto result = writeIndexFile(); !result)
            return std::unexpected(result.error());

        finished_ = true;
        return {};
    }


    size_t NamedResourceStoreWriter::size() const noexcept
    {
        return entries_.size();
    }


    bool NamedResourceStoreWriter::empty() const noexcept
    {
        return entries_.empty();
    }


    Result<void> NamedResourceStoreWriter::writeIndexFile() const
    {
        const auto directory = blobWriter_.directory();
        const auto idxPath = directory / "index.nidx";

        // build sorted order by ID for binary search when reading
        std::vector<size_t> sortedOrder(entries_.size());
        std::iota(sortedOrder.begin(), sortedOrder.end(), 0);
        std::ranges::sort(sortedOrder, [&](const size_t a, const size_t b) {
            return entries_[a].id < entries_[b].id;
        });

        // build records array in sorted order
        std::string idStrings;
        std::vector<NamedResourceStoreIndexRecord> records;
        records.reserve(entries_.size());

        for (const size_t idx : sortedOrder)
        {
            const auto& [id, format, fileSequence, fileOffset, length] = entries_[idx];
            const auto idOffset = static_cast<uint32_t>(idStrings.size());
            idStrings.append(id);
            idStrings.push_back('\0');

            NamedResourceStoreIndexRecord record {
                .format = format,
                .fileSequence = fileSequence,
                .idStringOffset = idOffset,
                .fileOffset = fileOffset,
                .length = length
            };
            records.push_back(record);
        }

        // build the ID strings region
        const size_t headerSize = HEADER_SIZE + records.size() * RECORD_SIZE;
        for (auto& record : records)
            record.idStringOffset += static_cast<uint32_t>(headerSize);

        std::ofstream file(idxPath, std::ios::binary | std::ios::trunc);
        if (!file)
            return std::unexpected(std::format("Failed to open: {}", idxPath.string()));

        NamedResourceStoreIndexHeader header{
            .zeroField = 0,
            .recordCount = static_cast<uint32_t>(records.size())
        };

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        file.write(reinterpret_cast<const char*>(records.data()),
                   static_cast<std::streamsize>(records.size() * sizeof(NamedResourceStoreIndexRecord)));
        file.write(idStrings.data(), static_cast<std::streamsize>(idStrings.size()));

        if (!file)
            return std::unexpected(std::format("Failed to write: {}", idxPath.string()));

        return {};
    }
}
