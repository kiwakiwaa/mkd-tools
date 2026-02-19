//
// kiwakiwaaにより 2026/02/11 に作成されました。
//

#include "monokakido/resource/keystore/keystore.hpp"

#include <bit>
#include <format>
#include <utility>


namespace monokakido
{
    void KeystoreHeader::swapEndianness() noexcept
    {
        version = std::byteswap(version);
        wordsOffset = std::byteswap(wordsOffset);
        idxOffset = std::byteswap(idxOffset);
        nextOffset = std::byteswap(nextOffset);
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


    Keystore::Keystore(
        std::vector<uint8_t>&& fileData,
        std::vector<uint32_t>&& indexLength,
        std::vector<uint32_t>&& indexPrefix,
        std::vector<uint32_t>&& indexSuffix,
        std::vector<uint32_t>&& indexD,
        const size_t wordsOffset,
        const std::span<const ConversionEntry> conversionTable,
        std::string dictId,
        std::string filename)
        : fileData_(std::move(fileData))
          , indexLength_(std::move(indexLength))
          , indexPrefix_(std::move(indexPrefix))
          , indexSuffix_(std::move(indexSuffix))
          , indexOther_(std::move(indexD))
          , wordsOffset_(wordsOffset)
          , conversionTable_(conversionTable)
          , dictId_(std::move(dictId))
          , filename_(std::move(filename))
    {
    }


    std::expected<Keystore, std::string>
    Keystore::load(const fs::path& path, const std::string& dictId)
    {
        auto readerResult = BinaryFileReader::open(path);
        if (!readerResult)
            return std::unexpected(readerResult.error());
        auto& reader = *readerResult;

        const size_t fileSize = reader.remainingBytes();

        // Parse file header
        auto headerResult = readHeader(reader);
        if (!headerResult)
            return std::unexpected(headerResult.error());
        const auto& header = *headerResult;

        // Read entire file into memory
        if (auto r = reader.seek(0); !r)
            return std::unexpected(r.error());

        auto fileDataResult = reader.readBytes(fileSize);
        if (!fileDataResult)
            return std::unexpected(fileDataResult.error());
        auto fileData = std::move(*fileDataResult);

        // Parse index section
        const size_t indexEnd = header.nextOffset != 0 ? header.nextOffset : fileSize;
        const auto indexSpan = std::span(fileData).subspan(header.idxOffset, indexEnd - header.idxOffset);

        auto idxHeaderResult = readIndexHeader(indexSpan, indexSpan.size());
        if (!idxHeaderResult)
            return std::unexpected(idxHeaderResult.error());

        auto indices = readAllIndices(indexSpan, *idxHeaderResult, indexSpan.size());
        if (!indices)
            return std::unexpected(indices.error());

        // Parse conversion table (v2 only)
        std::span<const ConversionEntry> conversionTable;
        if (header.nextOffset != 0)
        {
            const uint8_t* tableBase = fileData.data() + header.nextOffset;
            const size_t tableBytes = fileSize - header.nextOffset;

            if (tableBytes < 4)
                return std::unexpected("Conversion table header truncated");

            uint32_t tableCount;
            std::memcpy(&tableCount, tableBase, 4);
            if constexpr (std::endian::native == std::endian::big)
                tableCount = std::byteswap(tableCount);

            if (const size_t requiredBytes = 4 + static_cast<size_t>(tableCount) * sizeof(ConversionEntry);
                requiredBytes > tableBytes)
                return std::unexpected("Conversion table extends past end of file");

            auto* entries = reinterpret_cast<const ConversionEntry*>(tableBase + 4);
            conversionTable = std::span(entries, tableCount);
        }

        return Keystore(
            std::move(fileData),
            std::move(indices->length),
            std::move(indices->prefix),
            std::move(indices->suffix),
            std::move(indices->other),
            header.wordsOffset,
            conversionTable,
            dictId,
            path.filename().string()
        );
    }


    std::expected<KeystoreLookupResult, std::string> Keystore::getByIndex(
        const KeystoreIndex indexType, const size_t index) const
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
        if (!entry)
            return std::unexpected(entry.error());

        auto pages = decodePages(entry->pagesOffset);
        if (!pages)
            return std::unexpected(pages.error());

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


    std::string_view Keystore::filename() const
    {
        return filename_;
    }


    const std::vector<uint32_t>* Keystore::getIndexArray(const KeystoreIndex type) const noexcept
    {
        switch (type)
        {
            case KeystoreIndex::Length: return &indexLength_;
            case KeystoreIndex::Prefix: return &indexPrefix_;
            case KeystoreIndex::Suffix: return &indexSuffix_;
            case KeystoreIndex::Other: return &indexOther_;
        }
        std::unreachable();
    }


    std::expected<Keystore::WordEntry, std::string> Keystore::parseWordEntry(const uint32_t wordOffset) const
    {
        const size_t absOffset = wordsOffset_ + wordOffset;

        // Need at least: uint32 pagesOffset + 1 separator byte
        if (absOffset + sizeof(uint32_t) + 1 >= fileData_.size())
            return std::unexpected("Word offset out of bounds");

        uint32_t pagesOffset;
        std::memcpy(&pagesOffset, &fileData_[absOffset], sizeof(uint32_t));
        if constexpr (std::endian::native == std::endian::big)
            pagesOffset = std::byteswap(pagesOffset);

        // Skip past pagesOffset (4 bytes) + separator (1 byte)
        const size_t stringStart = absOffset + sizeof(uint32_t) + 1;

        // Find null terminator
        const auto* nullPos = static_cast<const uint8_t*>(
            std::memchr(&fileData_[stringStart], 0, fileData_.size() - stringStart)
        );
        if (!nullPos)
            return std::unexpected("Unterminated word string");

        const auto stringLen = static_cast<size_t>(nullPos - &fileData_[stringStart]);

        return WordEntry{
            .key = std::string_view(reinterpret_cast<const char*>(&fileData_[stringStart]), stringLen),
            .pagesOffset = pagesOffset,
        };
    }


    std::expected<std::vector<PageReference>, std::string> Keystore::decodePages(const size_t pagesOffset) const
    {
        const size_t absOffset = wordsOffset_ + pagesOffset;

        if (absOffset + 2 > fileData_.size())
            return std::unexpected(std::format(
                "Pages offset out of bounds: {} + 2 > {}", absOffset, fileData_.size()));

        return decodePageReferences(std::span(fileData_).subspan(absOffset));
    }


    // this should actually be determined by a var 'searchOption' but I don't know its origins
    bool Keystore::needsConversion() const noexcept
    {
        return !conversionTable_.empty() && (dictId_ == "KNEJ.EJ" || dictId_ == "KNJE.JE");
    }


    void Keystore::applyConversion(std::vector<PageReference>& refs) const noexcept
    {
        for (auto& [page, item] : refs)
        {
            if (page < conversionTable_.size())
            {
                const auto& mapped = conversionTable_[page];
                page = mapped.page;
                item = mapped.item;
            }
        }
    }


    std::expected<KeystoreHeader, std::string> Keystore::readHeader(BinaryFileReader& reader)
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
            if (!ext)
                return std::unexpected("Failed to read v2 header extension");
            std::memcpy(&header.nextOffset, &ext->version, 16);
        }

        if (header.magic1 != 0)
            return std::unexpected("Invalid magic1 field");

        if (header.wordsOffset >= header.idxOffset)
            return std::unexpected("Invalid offset ordering: wordsOffset >= idxOffset");

        if (header.version == KEYSTORE_V2)
        {
            if (header.magic5 != 0 || header.magic6 != 0 || header.magic7 != 0)
                return std::unexpected("Invalid magic fields in v2 header");

            if (header.nextOffset != 0 && header.idxOffset >= header.nextOffset)
                return std::unexpected("Invalid next offset in v2 header");
        }

        return header;
    }


    std::expected<IndexHeader, std::string> Keystore::readIndexHeader(const std::span<const uint8_t> data,
                                                                      const size_t maxSize)
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

        // Validate that offsets are monotonically ordered
        auto inOrder = [maxSize](const uint32_t a, const uint32_t b) {
            return b == 0 || a < b || b == maxSize;
        };

        if (!inOrder(header.indexAOffset, header.indexBOffset) ||
            !inOrder(header.indexBOffset, header.indexCOffset) ||
            !inOrder(header.indexCOffset, header.indexDOffset) ||
            !inOrder(header.indexDOffset, static_cast<uint32_t>(maxSize)))
        {
            return std::unexpected("Invalid index offset ordering");
        }

        return header;
    }


    std::expected<std::vector<uint32_t>, std::string> Keystore::readSingleIndex(
        const std::span<const uint8_t> data, const uint32_t start, const uint32_t end)
    {
        if (start == 0)
            return std::vector<uint32_t>{}; // index not present

        if (end <= start)
            return std::unexpected("Invalid index range");

        const size_t length = end - start;

        if (length < sizeof(uint32_t))
            return std::unexpected("Index too small");
        if (length % sizeof(uint32_t) != 0)
            return std::unexpected("Index size not aligned to 4 bytes");

        const size_t totalEntries = length / sizeof(uint32_t);
        std::vector<uint32_t> offsets(totalEntries);
        std::memcpy(offsets.data(), data.data() + start, length);

        if constexpr (std::endian::native == std::endian::big)
            for (auto& v : offsets)
                v = std::byteswap(v);

        // First element is the count; it must equal totalEntries - 1
        if (offsets[0] + 1 != totalEntries)
            return std::unexpected(std::format(
                "Index count mismatch: stored {} but have {} offsets",
                offsets[0], totalEntries - 1));

        // Remove the count, keep only the word offsets
        offsets.erase(offsets.begin());
        return offsets;
    }


    std::expected<Keystore::IndexArrays, std::string> Keystore::readAllIndices(
        std::span<const uint8_t> data,
        const IndexHeader& header,
        const size_t maxSize)
    {
        const auto sz = static_cast<uint32_t>(maxSize);

        auto a = readSingleIndex(data, header.indexAOffset, header.indexBOffset != 0 ? header.indexBOffset : sz);
        if (!a) return std::unexpected(std::format("Index A: {}", a.error()));

        auto b = readSingleIndex(data, header.indexBOffset, header.indexCOffset != 0 ? header.indexCOffset : sz);
        if (!b) return std::unexpected(std::format("Index B: {}", b.error()));

        auto c = readSingleIndex(data, header.indexCOffset, header.indexDOffset != 0 ? header.indexDOffset : sz);
        if (!c) return std::unexpected(std::format("Index C: {}", c.error()));

        auto d = readSingleIndex(data, header.indexDOffset, sz);
        if (!d) return std::unexpected(std::format("Index D: {}", d.error()));

        return IndexArrays{
            .length = std::move(*a),
            .prefix = std::move(*b),
            .suffix = std::move(*c),
            .other = std::move(*d),
        };
    }
}
