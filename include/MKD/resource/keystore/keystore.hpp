//
// kiwakiwaaにより 2026/02/07 に作成されました。
//

#pragma once

#include "MKD/resource/common.hpp"
#include "MKD/result.hpp"
#include "MKD/platform/binary_file_reader.hpp"
#include "MKD/resource/keystore/page_reference.hpp"
#include "MKD/resource/keystore/keystore_lookup_result.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace MKD
{
    /**
     * Keystore File Format (.keystore)
     * =================================
     *
     * Keystores map search terms (typically Japanese words) to locations
     * within dictionary content files. They enable efficient lookup of
     * dictionary entries by various keys (word form, reading, etc.).
     *
     * File Structure:
     * ┌─────────────────────────────────────────────────────────┐
     * │ File Header (16 or 32 bytes)                            │
     * │  - Version (4 bytes) — 0x10000 (v1) or 0x20000 (v2)     │
     * │  - Magic fields (must be 0)                             │
     * │  - Words offset                                         │
     * │  - Index offset                                         │
     * │  - Next offset (v2 only, points to conversion table)    │
     * ├─────────────────────────────────────────────────────────┤
     * │ Words Section (variable length)                         │
     * │  - uint32_le  pages_offset                              │
     * │  - uint8      separator (0x00)                          │
     * │  - Null-terminated UTF-8 search term                    │
     * │  - Page reference data (variable-length encoded)        │
     * ├─────────────────────────────────────────────────────────┤
     * │ Index Header (20 bytes)                                 │
     * │  - Magic (0x04)                                         │
     * │  - Four index offsets (A, B, C, D)                      │
     * ├─────────────────────────────────────────────────────────┤
     * │ Index Sections A, B, C, D (variable length each)        │
     * │  - uint32_le count                                      │
     * │  - uint32_le[] offsets into words section               │
     * ├─────────────────────────────────────────────────────────┤
     * │ Conversion Table (optional, v2 only)                    │
     * │  - uint32_le count                                      │
     * │  - ConversionEntry[count]                               │
     * └─────────────────────────────────────────────────────────┘
     */

    constexpr uint32_t KEYSTORE_V1 = 0x10000;
    constexpr uint32_t KEYSTORE_V2 = 0x20000;
    constexpr uint32_t INDEX_MAGIC = 0x04;

    enum class KeystoreIndex : size_t
    {
        Length = 0, // Index A — sorted by key length
        Prefix = 1, // Index B — sorted by key (prefix search)
        Suffix = 2, // Index C — sorted by key suffix
        Other = 3, // Index D — conversion table / other
    };

    struct KeystoreHeader : BinaryStruct<KeystoreHeader>
    {
        uint32_t version; // 0x10000 or 0x20000
        uint32_t magic1; // Must be 0
        uint32_t wordsOffset; // Offset to words section
        uint32_t indexOffset; // Offset to index section
        uint32_t conversionTableOffset; // Offset to conversion table (or 0)
        uint32_t magic5; // Must be 0
        uint32_t magic6; // Must be 0
        uint32_t magic7; // Must be 0

        [[nodiscard]] size_t headerSize() const noexcept;

        void swapEndianness() noexcept;
    };

    struct IndexHeader : BinaryStruct<IndexHeader>
    {
        uint32_t magic; // Must be 0x04
        uint32_t indexAOffset;
        uint32_t indexBOffset;
        uint32_t indexCOffset;
        uint32_t indexDOffset;

        void swapEndianness() noexcept;
    };

    struct ConversionEntry
    {
        uint32_t page;
        uint16_t item;
        uint16_t padding;
    };

    static_assert(sizeof(ConversionEntry) == 8);

    class Keystore
    {
    public:
        /**
         * Load a keystore file from disk.
         *
         * The entire file is read into memory. Index arrays and the optional
         * conversion table are parsed eagerly.
         *
         * @param path    Path to the .keystore file
         * @param dictId  Dictionary identifier (e.g. "KNEJ"). Used to decide
         *                whether automatic key-ID conversion is applied.
         * @return Loaded Keystore, or an error string
         */
        static Result<Keystore> load(const fs::path& path, const std::string& dictId);


        /**
         * Look up an entry by its position within an index.
         *
         * For dictionaries that require it (KNEJ, KNJE), page references are
         * automatically converted via the embedded conversion table.
         *
         * @param indexType  Which index to query
         * @param index      Position within that index
         * @return Lookup result with key and page references, or an error
         */
        [[nodiscard]] Result<KeystoreLookupResult> getByIndex(KeystoreIndex indexType, size_t index) const;

        /**
         * @return Number of entries in the given index (0 if the index doesn't exist).
         */
        [[nodiscard]] size_t indexSize(KeystoreIndex indexType) const noexcept;


        [[nodiscard]] std::string_view filename() const;

    private:
        Keystore(
            std::vector<uint8_t>&& fileData,
            std::vector<uint32_t>&& indexLength,
            std::vector<uint32_t>&& indexPrefix,
            std::vector<uint32_t>&& indexSuffix,
            std::vector<uint32_t>&& indexD,
            size_t wordsOffset,
            std::span<const ConversionEntry> conversionTable,
            std::string dictId,
            std::string filename
        );

        [[nodiscard]] const std::vector<uint32_t>* getIndexArray(KeystoreIndex type) const noexcept;

        /**
         * Parse a word entry from the words section.
         * Layout: uint32_le pages_offset | 0x00 | null-terminated string
         */
        struct WordEntry
        {
            std::string_view key;
            size_t pagesOffset; // words-section-relative
        };

        [[nodiscard]] Result<WordEntry> parseWordEntry(uint32_t wordOffset) const;

        /**
         * Decode page references at the given words-section-relative offset.
         */
        [[nodiscard]] Result<std::vector<PageReference>> decodePages(size_t pagesOffset) const;

        /**
         * @return true if this dictionary needs automatic key-ID conversion.
         */
        [[nodiscard]] bool needsConversion() const noexcept;

        /**
         * Apply the conversion table in-place.
         */
        void applyConversion(std::vector<PageReference>& refs) const noexcept;

        static Result<KeystoreHeader> readHeader(BinaryFileReader& reader);
        static Result<std::vector<uint8_t>> readFileData(const BinaryFileReader& reader, size_t fileSize);

        static Result<IndexHeader> readIndexHeader(std::span<const uint8_t> data);

        struct IndexArrays
        {
            std::vector<uint32_t> length, prefix, suffix, other;
        };

        static Result<IndexArrays> readAllIndices(std::span<const uint8_t> data, const IndexHeader& header);

        /**
         * Read a single index array.
         * Format: uint32_le count | uint32_le[count] offsets
         *
         * @param data   Buffer starting at the index section
         * @param start  Byte offset of this index within `data`
         * @param end    Byte offset of the next index (or section end)
         * @return Offset array (count element removed), or error
         */
        static Result<std::vector<uint32_t>> readSingleIndex(std::span<const uint8_t> data, uint32_t start, uint32_t end);

        static Result<std::span<const ConversionEntry>> parseConversionTable(std::span<const uint8_t> fileData, size_t offset, size_t fileSize);

        std::vector<uint8_t> fileData_;
        std::vector<uint32_t> indexLength_; // Index A
        std::vector<uint32_t> indexPrefix_; // Index B
        std::vector<uint32_t> indexSuffix_; // Index C
        std::vector<uint32_t> indexOther_; // Index D
        size_t wordsOffset_;

        std::span<const ConversionEntry> conversionTable_;
        std::string dictId_;
        std::string filename_;
    };
}
