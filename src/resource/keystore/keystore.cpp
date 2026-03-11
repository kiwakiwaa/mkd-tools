//
// kiwakiwaaにより 2026/02/11 に作成されました。
//

#include "MKD/resource/keystore.hpp"
#include "platform/binary_file_reader.hpp"

#include <array>
#include <bit>
#include <format>
#include <utility>


namespace MKD
{
    void KeystoreHeader::swapEndianness() noexcept
    {
        version = std::byteswap(version);
        wordsOffset = std::byteswap(wordsOffset);
        indexOffset = std::byteswap(indexOffset);
        conversionTableOffset = std::byteswap(conversionTableOffset);
    }

    size_t KeystoreHeader::headerSize() const noexcept
    {
        return version == KEYSTORE_V1 ? 16 : 32;
    }


    void IndexHeader::swapEndianness() noexcept
    {
        magic = std::byteswap(magic);
        indexAOffset = std::byteswap(indexAOffset);
        indexBOffset = std::byteswap(indexBOffset);
        indexCOffset = std::byteswap(indexCOffset);
        indexDOffset = std::byteswap(indexDOffset);
    }


    struct Keystore::Impl
    {
        std::vector<uint8_t> fileData;
        std::vector<uint32_t> indexLength; // Index A
        std::vector<uint32_t> indexPrefix; // Index B
        std::vector<uint32_t> indexSuffix; // Index C
        std::vector<uint32_t> indexOther; // Index D
        size_t wordsOffset;

        std::span<const ConversionEntry> conversionTable;
        std::string dictId;
        std::string filename;


        Impl(std::vector<uint8_t>&& fileData,
             std::vector<uint32_t>&& indexLength,
             std::vector<uint32_t>&& indexPrefix,
             std::vector<uint32_t>&& indexSuffix,
             std::vector<uint32_t>&& indexD,
             size_t wordsOffset,
             std::span<const ConversionEntry> conversionTable,
             std::string dictId,
             std::string filename)
            : fileData(std::move(fileData))
              , indexLength(std::move(indexLength))
              , indexPrefix(std::move(indexPrefix))
              , indexSuffix(std::move(indexSuffix))
              , indexOther(std::move(indexD))
              , wordsOffset(wordsOffset)
              , conversionTable(conversionTable)
              , dictId(std::move(dictId))
              , filename(std::move(filename))
        {
        }
    };


    Keystore::Keystore(std::unique_ptr<Impl> impl) noexcept
        : impl_(std::move(impl))
    {
    }


    Keystore::~Keystore() = default;
    Keystore::Keystore(Keystore&&) noexcept = default;
    Keystore& Keystore::operator=(Keystore&&) noexcept = default;


    namespace
    {
        Result<KeystoreHeader> readHeader(BinaryFileReader& reader)
        {
            auto result = reader.readStructPartial<KeystoreHeader>(16);
            if (!result)
                return std::unexpected(result.error());

            KeystoreHeader header = *result;

            if (header.version != KEYSTORE_V1 && header.version != KEYSTORE_V2)
                return std::unexpected(std::format("Invalid keystore version: 0x{:x}", header.version));

            // V2 has an additional 16 bytes
            if (header.version == KEYSTORE_V2)
            {
                const auto ext = reader.readStructPartial<KeystoreHeader>(16);
                if (!ext) return std::unexpected("Failed to read v2 header extension");
                std::memcpy(&header.conversionTableOffset, &ext->version, 16);
            }

            if (header.magic1 != 0)
                return std::unexpected("Invalid magic1 field");
            if (header.wordsOffset >= header.indexOffset)
                return std::unexpected("Invalid offset ordering: wordsOffset >= idxOffset");

            if (header.version == KEYSTORE_V2)
            {
                if (header.magic5 != 0 || header.magic6 != 0 || header.magic7 != 0)
                    return std::unexpected("Invalid magic fields in v2 header");
                if (header.conversionTableOffset != 0 && header.indexOffset >= header.conversionTableOffset)
                    return std::unexpected("Invalid next offset in v2 header");
            }

            return header;
        }


        Result<std::vector<uint8_t>> readFileData(const BinaryFileReader& reader, const size_t fileSize)
        {
            if (auto s = reader.seek(0); !s)
                return std::unexpected(s.error());

            return reader.readBytes(fileSize);
        }
    }


    Result<Keystore> Keystore::load(const fs::path& path, const std::string& dictId)
    {
        auto reader = BinaryFileReader::open(path);
        if (!reader) return std::unexpected(reader.error());

        const size_t fileSize = reader->remainingBytes();

        auto header = readHeader(*reader);
        if (!header) return std::unexpected(header.error());

        auto fileData = readFileData(*reader, fileSize);
        if (!fileData) return std::unexpected(fileData.error());

        const size_t indexEnd = header->conversionTableOffset != 0 ? header->conversionTableOffset : fileSize;
        auto indexSpan = std::span(*fileData).subspan(header->indexOffset, indexEnd - header->indexOffset);

        auto idxHeader = readIndexHeader(indexSpan);
        if (!idxHeader) return std::unexpected(idxHeader.error());

        auto indices = readAllIndices(indexSpan, *idxHeader);
        if (!indices) return std::unexpected(indices.error());

        auto convTable = parseConversionTable(*fileData, header->conversionTableOffset, fileSize);
        if (!convTable) return std::unexpected(convTable.error());


        return Keystore(std::make_unique<Impl>(
            std::move(*fileData),
            std::move(indices->length), std::move(indices->prefix),
            std::move(indices->suffix), std::move(indices->other),
            header->wordsOffset, *convTable, dictId, path.filename().string()
        ));
    }

    // todo: add light version maybe that doesnt decode pages (for search)
    Result<KeystoreLookupResult> Keystore::getByIndex(const KeystoreIndex indexType, const size_t index) const
    {
        const auto* arr = getIndexArray(indexType);
        if (!arr)
            return std::unexpected("Invalid index type");
        if (arr->empty())
            return std::unexpected("Index does not exist");
        if (index >= arr->size())
            return std::unexpected(std::format(
                "Index out of bounds: {} >= {}", index, arr->size()));

        // Parse the word entry (key string + pages offset)
        auto entry = parseWordEntry((*arr)[index]);
        if (!entry) return std::unexpected(entry.error());

        auto pages = decodePages(entry->pagesOffset);
        if (!pages) return std::unexpected(pages.error());

        // Apply conversion for KNEJ & KNJE
        if (needsConversion())
            applyConversion(*pages);

        return KeystoreLookupResult{
            .key = entry->key,
            .index = index,
            .pages = std::move(*pages),
        };
    }


    size_t Keystore::indexSize(const KeystoreIndex indexType) const noexcept
    {
        const auto* arr = getIndexArray(indexType);
        return arr ? arr->size() : 0;
    }


    std::string_view Keystore::filename() const { return impl_->filename; }


    const std::vector<uint32_t>* Keystore::getIndexArray(const KeystoreIndex type) const noexcept
    {
        switch (type)
        {
            case KeystoreIndex::Length: return &impl_->indexLength;
            case KeystoreIndex::Prefix: return &impl_->indexPrefix;
            case KeystoreIndex::Suffix: return &impl_->indexSuffix;
            case KeystoreIndex::Other: return &impl_->indexOther;
        }
        std::unreachable();
    }


    Result<Keystore::WordEntry> Keystore::parseWordEntry(const uint32_t wordOffset) const
    {
        const size_t absOffset = impl_->wordsOffset + wordOffset;

        // Need at least: uint32 pagesOffset + 1 separator byte
        if (absOffset + sizeof(uint32_t) + 1 >= impl_->fileData.size())
            return std::unexpected("Word offset out of bounds");

        uint32_t pagesOffset;
        std::memcpy(&pagesOffset, &impl_->fileData[absOffset], sizeof(uint32_t));
        if constexpr (std::endian::native == std::endian::big)
            pagesOffset = std::byteswap(pagesOffset);

        // Skip past pagesOffset (4 bytes) + separator (1 byte)
        const size_t stringStart = absOffset + sizeof(uint32_t) + 1;

        // Find null terminator
        const auto* nullPos = static_cast<const uint8_t*>(
            std::memchr(&impl_->fileData[stringStart], 0, impl_->fileData.size() - stringStart)
        );
        if (!nullPos)
            return std::unexpected("Unterminated word string");

        const auto stringLen = static_cast<size_t>(nullPos - &impl_->fileData[stringStart]);

        return WordEntry{
            .key = std::string_view(reinterpret_cast<const char*>(&impl_->fileData[stringStart]), stringLen),
            .pagesOffset = pagesOffset,
        };
    }


    Result<std::vector<PageReference> > Keystore::decodePages(const size_t pagesOffset) const
    {
        const size_t absOffset = impl_->wordsOffset + pagesOffset;

        if (absOffset + 2 > impl_->fileData.size())
            return std::unexpected(std::format(
                "Pages offset out of bounds: {} + 2 > {}", absOffset, impl_->fileData.size()));

        return decodePageReferences(std::span(impl_->fileData).subspan(absOffset));
    }


    // this should actually be determined by a var 'searchOption' but I don't know its origins
    bool Keystore::needsConversion() const noexcept
    {
        return !impl_->conversionTable.empty() && (impl_->dictId == "KNEJ.EJ" || impl_->dictId == "KNJE.JE");
    }


    void Keystore::applyConversion(std::vector<PageReference>& refs) const noexcept
    {
        for (auto& [page, item] : refs)
        {
            if (page < impl_->conversionTable.size())
            {
                const auto& mapped = impl_->conversionTable[page];
                page = mapped.page;
                item = mapped.item;
            }
        }
    }


    Result<IndexHeader> Keystore::readIndexHeader(const std::span<const uint8_t> data)
    {
        if (data.size() < sizeof(IndexHeader))
            return std::unexpected("Index header truncated");

        IndexHeader header{};
        std::memcpy(&header, data.data(), sizeof(IndexHeader));
        if constexpr (std::endian::native == std::endian::big)
            header.swapEndianness();

        if (header.magic != INDEX_MAGIC)
            return std::unexpected(std::format(
                "Invalid index magic: expected 0x04, got 0x{:x}", header.magic));

        return header;
    }


    Result<std::vector<uint32_t> > Keystore::readSingleIndex(
        const std::span<const uint8_t> data,
        const uint32_t start,
        const uint32_t end)
    {
        if (start == 0)
            return std::vector<uint32_t>{}; // index not present

        if (end <= start)
            return std::unexpected("Invalid index range");

        const size_t length = end - start;
        if (length < sizeof(uint32_t) || length % sizeof(uint32_t) != 0)
            return std::unexpected("Invalid index size");

        const size_t totalEntries = length / sizeof(uint32_t);
        std::vector<uint32_t> offsets(totalEntries);
        std::memcpy(offsets.data(), data.data() + start, length);

        if constexpr (std::endian::native == std::endian::big)
            for (auto& v : offsets)
                v = std::byteswap(v);

        // First element is the count, it must equal totalEntries - 1
        if (offsets[0] + 1 != totalEntries)
            return std::unexpected(std::format(
                "Index count mismatch: stored {} but have {} offsets",
                offsets[0], totalEntries - 1));

        // Remove the count, keep only the word offsets
        offsets.erase(offsets.begin());
        return offsets;
    }


    Result<Keystore::IndexArrays> Keystore::readAllIndices(
        std::span<const uint8_t> data,
        const IndexHeader& header)
    {
        const auto sz = static_cast<uint32_t>(data.size());

        // Each index ends where the next one begins (or at section end)
        const std::array offsets = {
            header.indexAOffset, header.indexBOffset,
            header.indexCOffset, header.indexDOffset, sz
        };

        // Resolve the end boundary for index i: the first nonzero offset after it, or sz
        auto endOf = [&](const size_t i) -> uint32_t {
            for (size_t j = i + 1; j < offsets.size(); ++j)
                if (offsets[j] != 0) return offsets[j];
            return sz;
        };

        auto a = readSingleIndex(data, offsets[0], endOf(0));
        if (!a) return std::unexpected(std::format("Index A: {}", a.error()));

        auto b = readSingleIndex(data, offsets[1], endOf(1));
        if (!b) return std::unexpected(std::format("Index B: {}", b.error()));

        auto c = readSingleIndex(data, offsets[2], endOf(2));
        if (!c) return std::unexpected(std::format("Index C: {}", c.error()));

        auto d = readSingleIndex(data, offsets[3], endOf(3));
        if (!d) return std::unexpected(std::format("Index D: {}", d.error()));

        return IndexArrays{
            .length = std::move(*a),
            .prefix = std::move(*b),
            .suffix = std::move(*c),
            .other = std::move(*d),
        };
    }


    Result<std::span<const ConversionEntry> > Keystore::parseConversionTable(
        const std::span<const uint8_t> fileData,
        const size_t offset,
        const size_t fileSize)
    {
        if (offset == 0)
            return std::span<const ConversionEntry>{};

        const size_t tableBytes = fileSize - offset;
        if (tableBytes < 4)
            return std::unexpected("Conversion table header truncated");

        uint32_t count;
        std::memcpy(&count, fileData.data() + offset, 4);
        if constexpr (std::endian::native == std::endian::big)
            count = std::byteswap(count);

        const size_t required = 4 + static_cast<size_t>(count) * sizeof(ConversionEntry);
        if (required > tableBytes)
            return std::unexpected("Conversion table extends past end of file");

        auto* entries = reinterpret_cast<const ConversionEntry*>(fileData.data() + offset + 4);
        return std::span(entries, count);
    }
}
