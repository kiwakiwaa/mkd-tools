//
// kiwakiwaaにより 2026/01/19 に作成されました。
//

#pragma once

#include "monokakido/resource/common.hpp"
#include "rsc_index_record.hpp"
#include "rsc_map.hpp"

#include <expected>
#include <iterator>
#include <filesystem>
#include <vector>


namespace fs = std::filesystem;

namespace monokakido::resource
{
    /**
     * RSC Index File Format (contents.idx + contents.map)
     * ====================================================
     *
     * The RSC index system uses a two-file approach to map item IDs to their
     * locations in .rsc data files. The design separates ID management from
     * location data.
     *
     * File Structure Overview:
     *
     * contents.idx (Optional - ID to Index mapping):
     * ┌─────────────────────────────────────────────────────────────────┐
     * │ Header (8 bytes)                                                │
     * │  ├─ length (4 bytes): Number of IdxRecords                      │
     * │  └─ padding (4 bytes): Reserved/unused                          │
     * ├─────────────────────────────────────────────────────────────────┤
     * │ IdxRecords Array (length × 8 bytes)                             │
     * │  Each record contains:                                          │
     * │   ├─ itemId (4 bytes): Custom item ID                           │
     * │   └─ mapIdx (4 bytes): Index into contents.map                  │
     * └─────────────────────────────────────────────────────────────────┘
     *
     * contents.map (Required - Index to Location mapping)
     * ┌─────────────────────────────────────────────────────────────────┐
     * │ Header (8 bytes)                                                │
     * │  ├─ version (4 bytes): Format version                           │
     * │  └─ recordCount (4 bytes): Number of MapRecords                 │
     * ├─────────────────────────────────────────────────────────────────┤
     * │ MapRecords Array (recordCount × 8 bytes)                        │
     * │  Each record contains:                                          │
     * │   ├─ zOffset (4 bytes): Global offset to compressed chunk       │
     * │   └─ ioffset (4 bytes): Offset within decompressed chunk        │
     * └─────────────────────────────────────────────────────────────────┘
     *
     * Dictionaries typical have both .idx and a .map
     * Sequential data like fonts only have a .map file
     *
     * Lookup Process:
    *
     * WITH idx file:
     * ┌─────────┐     ┌─────────────┐     ┌─────────────┐     ┌──────────┐
     * │ Item ID │ --> │ contents.idx│ --> │ Map Index   │ --> │ Location │
     * │  12345  │     │ IdxRecord   │     │      7      │     │  in .rsc │
     * └─────────┘     └─────────────┘     └─────────────┘     └──────────┘
     *                  Binary search       Direct access       via MapRecord
     *
     * WITHOUT idx file:
     * ┌─────────┐     ┌─────────────┐     ┌──────────┐
     * │ Item ID │ --> │ Map Index   │ --> │ Location │
     * │    7    │     │   (same)    │     │  in .rsc │
     * └─────────┘     └─────────────┘     └──────────┘
     *                  Direct use          via MapRecord
     *
     */
    struct IdxHeader : BinaryStruct<IdxHeader>
    {
        uint32_t length;    // Number of IdxRecords in the file
        uint32_t padding;

        void swapEndianness() noexcept;
    };


    class RscIndex
    {
    public:
        static std::expected<RscIndex, std::string> load(const fs::path& directoryPath);

        [[nodiscard]] std::expected<MapRecord, std::string> findById(uint32_t itemId) const;

        [[nodiscard]] std::expected<std::pair<uint32_t, MapRecord>, std::string> getByIndex(size_t index) const;

        [[nodiscard]] size_t size() const noexcept;

        [[nodiscard]] bool empty() const noexcept;

        class Iterator
        {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = std::pair<uint32_t, MapRecord>;
            using pointer = const value_type*;
            using reference = value_type; // not reference since we return by value

            Iterator() noexcept = default;

            Iterator(const RscIndex* index, size_t pos);

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
            const RscIndex* index_ = nullptr;
            size_t position_ = 0;
        };

        [[nodiscard]] Iterator begin() const;

        [[nodiscard]] Iterator end() const;

        static_assert(std::random_access_iterator<Iterator>);

    private:
        RscIndex(std::vector<IdxRecord>&& idxRecords, std::vector<MapRecord>&& mapRecords);

        static std::expected<std::vector<MapRecord>, std::string> loadMapFile(const fs::path& directoryPath);

        static std::expected<std::vector<IdxRecord>, std::string> loadIdxFile(const fs::path& directoryPath);

        std::vector<IdxRecord> idxRecords_;
        std::vector<MapRecord> mapRecords_;
    };
}
