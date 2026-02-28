//
// kiwakiwaaにより 2026/01/19 に作成されました。
//

#pragma once

#include "MKD/resource/common.hpp"
#include "MKD/result.hpp"
#include "MKD/resource/rsc/rsc_index_record.hpp"
#include "MKD/resource/rsc/rsc_map.hpp"

#include <expected>
#include <iterator>
#include <filesystem>
#include <vector>


namespace fs = std::filesystem;

namespace MKD
{
    /**
     * RSC Index File Format (contents.idx + contents.map)
     *
     * The RSC index system uses a two files to map item IDs to their
     * locations in .rsc data files. The design separates ID management from
     * location data.
     *
     * File Structure Overview:
     *
     * contents.idx (Optional - ID to Index mapping):
     * ┌─────────────────────────────────────────────────────────────────┐
     * │ Header (8 bytes)                                                │
     * │  ├─ length (4 bytes): Number of IdxRecords                      │
     * │  └─ padding (4 bytes): unused                                   │
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
     * with idx file:
     * ┌─────────┐     ┌─────────────┐     ┌─────────────┐     ┌──────────┐
     * │ Item ID │ --> │ contents.idx│ --> │ Map Index   │ --> │ Location │
     * │  12345  │     │ IdxRecord   │     │      7      │     │  in .rsc │
     * └─────────┘     └─────────────┘     └─────────────┘     └──────────┘
     *                  Binary search       Direct access       via MapRecord
     *
     * only map file:
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
        static Result<RscIndex> load(const fs::path& directoryPath);

        [[nodiscard]] Result<MapRecord> findById(uint32_t itemId) const;

        [[nodiscard]] Result<std::pair<uint32_t, MapRecord>> getByIndex(size_t index) const;

        [[nodiscard]] uint32_t mapVersion() const;

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
        RscIndex(uint32_t mapVersion, std::optional<std::vector<IdxRecord>>&& idxRecords, std::vector<MapRecord>&& mapRecords);

        static Result<std::pair<std::vector<MapRecord>, uint32_t>> loadMapFile(const fs::path& directoryPath);

        static Result<std::optional<std::vector<IdxRecord>>> loadIdxFile(const fs::path& directoryPath);

        std::optional<std::vector<IdxRecord>> idxRecords_;
        std::vector<MapRecord> mapRecords_;
        uint32_t mapVersion_ = 0;
    };
}
