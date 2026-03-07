//
// kiwakiwaaにより 2026/03/07 に作成されました。
//

#pragma once

#include "MKD/result.hpp"
#include "MKD/resource/keystore.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace MKD
{
    class KeystoreWriter
    {
    public:
        explicit KeystoreWriter(uint32_t version = KEYSTORE_V1);

        /**
         * Add a search key mapping to one or more page references
         * If the key already exists, the page references are merged
         *
         * @param key key to add
         * @param pages page references that key should map to
         * @return Void or error string
         */
        Result<void> add(std::string_view key, std::vector<PageReference> pages);

        /**
         * Add multiple search keys that all point to the same page
         *
         * @param page page reference to add the keys to
         * @param keys vector of keys to add
         * @return Void or error string
         */
        Result<void> addPage(PageReference page, std::span<const std::string_view> keys);

        /**
         * Write the .keystore file to disk
         * Entries are sorted into the appropriate index arrays
         *
         * @param filePath Output .keystore file path
         */
        [[nodiscard]] Result<void> finalize(const fs::path& filePath) const;

        [[nodiscard]] size_t size() const noexcept;
        [[nodiscard]] bool empty() const noexcept;

    private:
        struct Entry
        {
            std::string key;
            std::vector<PageReference> pages;
        };

        /**
         * Collect flattened and sorted entries ready for serialisation
         *
         * @return flattened Keystore::Entry entries
         */
        [[nodiscard]] std::vector<Entry> collectEntries() const;

        /**
         * Encode page references into the variable-length binary format
         *
         * @param pages pages to encode
         * @return vector of encoded pages for the word section
         */
        static std::vector<uint8_t> encodePages(const std::vector<PageReference>& pages);

        struct WordsSection
        {
            std::vector<uint8_t> data;
            std::vector<uint32_t> entryOffsets;
        };

        /**
         * Build the words section and record each entry's offset
         * entry layout: uint32_le pages_offset | 0x00 separator | key\0 | encoded_pages
         *
         * @param entries
         * @return WordsSection or error string
         */
        [[nodiscard]] static Result<WordsSection> buildWordsSection(const std::vector<Entry>& entries);

        /**
         * Build sorted index arrays. Each contains offsets into the words section.
         *
         * - Length:  sorted by key byte length (ascending), then lexicographic
         * - Prefix:  sorted lexicographically (enables prefix binary search)
         * - Suffix:  sorted by reversed key (enables suffix search)
         * - Other:   same as prefix (placeholder)
         */
        struct IndexArrays
        {
            std::vector<uint32_t> length;
            std::vector<uint32_t> prefix;
            std::vector<uint32_t> suffix;
            std::vector<uint32_t> other;
        };

        [[nodiscard]] static IndexArrays buildIndexArrays(
            const std::vector<Entry>& entries,
            const WordsSection& words);

        static std::vector<uint8_t> serializeIndexArray(const std::vector<uint32_t>& offsets);

        uint32_t version_;
        std::unordered_map<std::string, std::vector<PageReference>> entries_;
    };
}