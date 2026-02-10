//
// kiwakiwaaにより 2026/01/16 に作成されました。
//

#pragma once

#include "nrsc_index_record.hpp"
#include "monokakido/resource/common.hpp"

#include <expected>
#include <filesystem>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace monokakido
{
    /**
     * NRSC Index File Format (.nidx)
     * ==============================
     *
     * The index.nidx file acts as a table of contents for .nrsc resource files.
     * It maps string IDs to their locations in the numbered resource files.
     *
     * File Structure:
     * ┌─────────────────────────────────────────────────────────────┐
     * │ Header (8 bytes)                                            │
     * │  - Zero field (4 bytes)                                     │
     * │  - Record count (4 bytes, little-endian uint32)             │
     * ├─────────────────────────────────────────────────────────────┤
     * │ Records Array (count × 16 bytes)                            │
     * │  Each record contains:                                      │
     * │   - Compression format (2 bytes)                            │
     * │   - File sequence number (2 bytes) - which .nrsc file       │
     * │   - ID string offset (4 bytes) - offset into strings region │
     * │   - File offset (4 bytes) - offset within the .nrsc file    │
     * │   - Data length (4 bytes) - size of resource data           │
     * ├─────────────────────────────────────────────────────────────┤
     * │ ID Strings Region (variable length)                         │
     * │  - Null-terminated strings concatenated together            │
     * │  - Referenced by idStringOffset in records                  │
     * └─────────────────────────────────────────────────────────────┘
     *
     * Example:
     * - Record with fileSequence=0, fileOffset=1024 -> points to byte 1024 in "0.nrsc"
     * - Record with idStringOffset=50 -> string starts at byte 50 in strings region
     */


    constexpr size_t HEADER_SIZE = 8;
    constexpr size_t RECORD_SIZE = sizeof(NrscIndexRecord);

    struct IndexHeader : BinaryStruct<IndexHeader>
    {
        uint32_t zeroField;
        uint32_t recordCount;

        void swapEndianness() noexcept;
    };


    // Manages the nrsc index file (index.nidx)
    class NrscIndex
    {
    public:
        /**
         * Factory method to load an Nrsc index file from a directory (index.nidx)
         * @param directoryPath Directory containing the index
         * @return NrscIndex or error string if failure
         */
        static std::expected<NrscIndex, std::string> load(const fs::path& directoryPath);


        /**
         * Find a record by string ID (binary search)
         * @return NrscIndexRecord or error string if failure
         */
        [[nodiscard]] std::expected<NrscIndexRecord, std::string> findById(std::string_view id) const;


        /**
         * Get record by index
         * @param index Index of record
         * @return string ID & NrscIndexRecord pair or error string if failure
         */
        [[nodiscard]] std::expected<std::pair<std::string_view, NrscIndexRecord>, std::string> getByIndex(size_t index) const;


        /**
         * Get total number of records
         * @return Number of records
         */
        [[nodiscard]] size_t size() const noexcept;


        /**
         * Check if NrscIndex is empty
         * @return true if empty
         */
        [[nodiscard]] bool empty() const noexcept;


        class Iterator
        {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using iterator_concept = std::random_access_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = std::pair<std::string_view, NrscIndexRecord>;
            using pointer = const value_type*;
            using reference = value_type; // not reference since we return by value

            Iterator() noexcept = default;
            Iterator(const NrscIndex* index, size_t pos);

            value_type operator*() const;
            value_type operator[](difference_type n) const;

            Iterator& operator++();
            Iterator operator++(int);
            Iterator& operator--();
            Iterator operator--(int);

            Iterator& operator+=(difference_type n);
            Iterator& operator-=(difference_type n);
            Iterator operator+(difference_type n) const;
            Iterator operator-(difference_type n) const;

            friend Iterator operator+(difference_type n, const Iterator& it);
            difference_type operator-(const Iterator& other) const;

            auto operator<=>(const Iterator& other) const = default;
            bool operator==(const Iterator& other) const = default;

        private:
            const NrscIndex* index_ = nullptr;
            size_t position_ = 0;
        };

        [[nodiscard]] Iterator begin() const;

        [[nodiscard]] Iterator end() const;

        static_assert(std::random_access_iterator<Iterator>);


    private:

        NrscIndex(std::vector<NrscIndexRecord>&& records, std::string&& idStrings, size_t headerSize);


        /**
         * Get ID string at given offset
         * @param offset offset in index file
         * @return ID string or error string if failure
         */
        [[nodiscard]] std::expected<std::string_view, std::string> getIdAt(size_t offset) const;


        std::vector<NrscIndexRecord> records_;  // All index records, sorted by ID
        std::string idStrings_;                 // Concatenated null-terminated ID strings
        size_t headerSize_;                     // Header + records size (for offset calculations)
    };

};
