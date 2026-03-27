//
// kiwakiwaaにより 2026/02/12 に作成されました。
//

#pragma once

#include "MKD/result.hpp"
#include "MKD/resource/common.hpp"
#include "MKD/resource/headline_record.hpp"
#include "platform/mmap_file.hpp"

#include <expected>
#include <iterator>
#include <string>
#include <string_view>


namespace MKD
{
    /**
     * Headlines File Format
     * =====================
     *
     * The headlines file maps entry IDs to display strings used as dictionary
     * headwords. Each entry can have a headline, optional prefix, and optional
     * suffix that are concatenated to form the full display string.
     *
     * File Structure:
     * ┌──────────────────────────────────────────────────────────────┐
     * │ Header                                                       │
     * │  - Reserved        (8 bytes)                                 │
     * │  - Entry count     (4 bytes, uint32 LE)                      │
     * │  - Records offset  (4 bytes, uint32 LE, from file start)     │
     * │  - Strings offset  (4 bytes, uint32 LE, from file start)     │
     * │  - Record stride   (4 bytes, uint32 LE, 0 = default 20)      │
     * ├──────────────────────────────────────────────────────────────┤
     * │ Records Array (count × stride bytes)                         │
     * │  Sorted by entry ID for binary search.                       │
     * │  See HeadlineRecord for per-record layout.                   │
     * ├──────────────────────────────────────────────────────────────┤
     * │ Strings Region (variable length)                             │
     * │  Null-terminated UTF-16LE strings, referenced by byte offset │
     * └──────────────────────────────────────────────────────────────┘
     *
     * Entry IDs are 48-bit composite keys (32-bit pageId + 16-bit itemId).
     */

    constexpr size_t HEADLINE_DEFAULT_STRIDE = 20;
    constexpr size_t HEADLINE_EXTENDED_STRIDE = 24;


    struct HeadlineHeader
    {
        uint8_t reserved[8];
        uint32_t entryCount;
        uint32_t recordsOffset;
        uint32_t stringsOffset;
        uint32_t recordStride;  // 0 → use HEADLINE_DEFAULT_STRIDE

        [[nodiscard]] uint32_t effectiveStride() const noexcept;
    };


    struct HeadlineComponents
    {
        std::u16string_view prefix;     // empty if absent
        std::u16string_view headline;
        std::u16string_view suffix;     // empty if absent
        EntryId entryId;

        [[nodiscard]] std::u16string full() const;
        [[nodiscard]] std::string fullUtf8() const;

        [[nodiscard]] std::string prefixUtf8() const;
        [[nodiscard]] std::string headlineUtf8() const;
        [[nodiscard]] std::string suffixUtf8() const;
    };


    /**
     * HeadlineStore
     *
     * Provides lookup of display headwords for dictionary entries.
     * Entries are sorted by ID for efficient binary search.
     */
    class HeadlineStore
    {
    public:
        /**
         * Load a headlines file from the given path
         * @param filePath Path to the headlines file
         * @return HeadlineStore or error string
         */
        static Result<HeadlineStore> load(const fs::path& filePath);


        /**
         * Get decomposed headline components at a given index.
         * @param index Zero-based record index
         * @return HeadlineComponents or error if out of range / malformed
         */
        [[nodiscard]] Result<HeadlineComponents> operator[](size_t index) const;

        /**
         * Get the packed 48-bit entry ID at a given index.
         * @param index Zero-based record index
         * @return Packed entry ID, or error if out of range
         */
        [[nodiscard]] Result<EntryId> entryIdAt(size_t index) const;


        /**
         * Get number of headline entries
         */
        [[nodiscard]] size_t size() const noexcept;


        /**
         * Check if the store is empty
         */
        [[nodiscard]] bool empty() const noexcept;


        [[nodiscard]] std::string_view filename() const;


        class Iterator
        {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using iterator_concept  = std::random_access_iterator_tag;
            using difference_type   = std::ptrdiff_t;
            using value_type        = HeadlineComponents;
            using reference         = value_type;   // returns by value

            Iterator() noexcept = default;
            Iterator(const HeadlineStore* store, size_t pos) noexcept;

            value_type operator*() const;
            value_type operator[](difference_type n) const;

            Iterator& operator++() noexcept;
            Iterator  operator++(int) noexcept;
            Iterator& operator--() noexcept;
            Iterator  operator--(int) noexcept;

            Iterator& operator+=(difference_type n) noexcept;
            Iterator& operator-=(difference_type n) noexcept;
            Iterator  operator+(difference_type n) const noexcept;
            Iterator  operator-(difference_type n) const noexcept;

            friend Iterator operator+(difference_type n, const Iterator& it) noexcept;
            difference_type operator-(const Iterator& other) const noexcept;

            auto operator<=>(const Iterator& other) const = default;
            bool operator==(const Iterator& other) const = default;

        private:
            const HeadlineStore* store_ = nullptr;
            size_t position_ = 0;
        };

        [[nodiscard]] Iterator begin() const noexcept;
        [[nodiscard]] Iterator end() const noexcept;

        static_assert(std::random_access_iterator<Iterator>);


    private:

        HeadlineStore(
            MappedFile&& fileData,
            std::string filename,
            uint32_t entryCount,
            uint32_t stride,
            size_t recordsOffset,
            size_t stringsOffset);


        /**
         * Get pointer to the record at the given index
         */
        [[nodiscard]] const uint8_t* recordAt(size_t index) const noexcept;


        /**
         * Read a null-terminated UTF-16LE string view at a byte offset in the strings region
         * @param offset Byte offset from strings region start
         * @return String view into the mapped data, or error
         */
        [[nodiscard]] Result<std::u16string_view> stringAt(uint32_t offset) const;


        /**
         * Build HeadlineComponents from a raw record pointer
         */
        [[nodiscard]] Result<HeadlineComponents> componentsFromRecord(const uint8_t* record) const;


        MappedFile fileData_;
        std::string filename_;
        uint32_t entryCount_;
        uint32_t stride_;
        size_t recordsOffset_;
        size_t stringsOffset_;
    };
}
