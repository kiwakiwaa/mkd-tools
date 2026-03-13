//
// kiwakiwaaにより 2026/02/07 に作成されました。
//

#pragma once

#include "MKD/result.hpp"
#include "MKD/resource/common.hpp"
#include "MKD/resource/entry_id.hpp"

#include <expected>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace MKD
{
    /**
     * Keystore File Format (.keystore)
     * =================================
     *
     * Keystores map search terms to locations within dictionary content files.
     *
     * File Structure:
     * ┌─────────────────────────────────────────────────────────┐
     * │ File Header                                             │
     * ├─────────────────────────────────────────────────────────┤
     * │ Words Section                                           │
     * ├─────────────────────────────────────────────────────────┤
     * │ Index Section                                           │
     * ├─────────────────────────────────────────────────────────┤
     * │ Conversion Table (optional, v2 only)                    │
     * └─────────────────────────────────────────────────────────┘
     *
     * FILE HEADER
     * -----------
     * Two versions exist, distinguished by the version field:
     *
     * V1 header (16 bytes):
     * ┌─────────────────────────────────────────────────────────┐
     * │ version          (4 bytes)  0x10000                     │
     * │ magic1           (4 bytes)  must be 0                   │
     * │ wordsOffset      (4 bytes)  offset to words section     │
     * │ indexOffset      (4 bytes)  offset to index section     │
     * └─────────────────────────────────────────────────────────┘
     *
     * V2 header (32 bytes):
     * ┌─────────────────────────────────────────────────────────┐
     * │ version          (4 bytes)  0x20000                     │
     * │ magic1           (4 bytes)  must be 0                   │
     * │ wordsOffset      (4 bytes)  offset to words section     │
     * │ indexOffset       (4 bytes)  offset to index section    │
     * │ convTableOffset  (4 bytes)  offset to conversion table  │
     * │ magic5           (4 bytes)  must be 0                   │
     * │ magic6           (4 bytes)  must be 0                   │
     * │ magic7           (4 bytes)  must be 0                   │
     * └─────────────────────────────────────────────────────────┘
     *
     * WORDS SECTION
     * -------------
     * Contains word entries and page reference blocks. Index arrays point into this section via word offsets.
     * Word entry:
     * ┌─────────────────────────────────────────────────────────┐
     * │ pagesOffset      (4 bytes)  words-section-relative      │
     * │ separator         (1 byte)   0x00                       │
     * │ key               (variable) null-terminated UTF-8      │
     * └─────────────────────────────────────────────────────────┘
     *
     * Entry ids:
     * ┌─────────────────────────────────────────────────────────┐
     * │ Variable-length encoded entry ids                       │
     * │ (decoded by decodeEntryIds)                             │
     * └─────────────────────────────────────────────────────────┘
     *
     * INDEX SECTION
     * -------------
     * Begins at indexOffset. Contains a header followed by up to
     * four index arrays, each providing a different sort order
     * over the word entries.
     *
     * Index header (20 bytes):
     * ┌─────────────────────────────────────────────────────────┐
     * │ magic            (4 bytes)  must be 0x04                │
     * │ indexAOffset     (4 bytes)  relative to index section   │
     * │ indexBOffset     (4 bytes)  relative to index section   │
     * │ indexCOffset     (4 bytes)  relative to index section   │
     * │ indexDOffset     (4 bytes)  relative to index section   │
     * └─────────────────────────────────────────────────────────┘
     *
     * An offset of 0 means that index is not present.
     *
     * Each index array:
     * ┌─────────────────────────────────────────────────────────┐
     * │ count            (4 bytes)  number of entries           │
     * │ offsets          (4 bytes × count)                      │
     * │                  each is a word offset into the words   │
     * │                  section                                │
     * └─────────────────────────────────────────────────────────┘
     *
     * Index A — sorted by key length
     * Index B — sorted by key (for prefix search)
     * Index C — sorted by key suffix
     * Index D — other / conversion-related
     *
     *
     * CONVERSION TABLE (optional, v2 only)
     * ------------------------------------
     * Located at convTableOffset. Used by specific dictionaries
     * (KNEJ, KNJE, maybe others too...) to remap entry ids after lookup.
     */

    constexpr uint32_t KEYSTORE_V1 = 0x10000;
    constexpr uint32_t KEYSTORE_V2 = 0x20000;

    enum class KeystoreIndex : size_t
    {
        Length = 0, // Index A — sorted by key length
        Prefix = 1, // Index B — sorted by key (prefix search)
        Suffix = 2, // Index C — sorted by key suffix
        Other  = 3, // Index D — conversion table / other
    };

    struct KeystoreEntry
    {
        std::string_view key;
        size_t index = 0;

        [[nodiscard]] bool empty() const noexcept { return key.empty(); }
    };

    class Keystore
    {
    public:
        class Iterator;

        static Result<Keystore> open(const std::filesystem::path& path, std::string dictId);

        ~Keystore();
        Keystore(Keystore&&) noexcept;
        Keystore& operator=(Keystore&&) noexcept;
        Keystore(const Keystore&) = delete;
        Keystore& operator=(const Keystore&) = delete;

        /**
         * Look up an entry by its position within an index.
         *
         * For dictionaries that require it (KNEJ, KNJE), entry ids are
         * automatically converted via the embedded conversion table
         *
         * @param type Which index to query
         * @param index Position within that index
         * @return Lookup result with key and entry ids, or an error
         */
        [[nodiscard]] Result<KeystoreEntry> entryAt(KeystoreIndex type, size_t index) const;

        /**
         * Lookup key only, without decoding entry ids.
         *
         * @param type Which index to query
         * @param index Position within that index
         * @return Lookup key with its index, or an error
         */
        [[nodiscard]] Result<std::string_view> keyAt(KeystoreIndex type, size_t index) const;

        /**
         * Decode entry ids for a previously located keystore position
         *
         * @param type Which index to query
         * @param index Position within that index
         * @return Entry ids for a keystore position, or an error
         */
        [[nodiscard]] Result<std::vector<EntryId>> entryIdsAt(KeystoreIndex type, size_t index) const;

        /// Number of entries in the given index (0 if absent).
        [[nodiscard]] size_t indexSize(KeystoreIndex type) const noexcept;

        [[nodiscard]] std::string_view filename() const noexcept;

        [[nodiscard]] Iterator begin(KeystoreIndex type) const noexcept;
        [[nodiscard]] Iterator end(KeystoreIndex type) const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
        explicit Keystore(std::unique_ptr<Impl> impl) noexcept;
    };

    class Keystore::Iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = KeystoreEntry;
        using difference_type = std::ptrdiff_t;

        Iterator() = default;

        [[nodiscard]] value_type operator*() const;
        Iterator& operator++();
        Iterator operator++(int);

        [[nodiscard]] bool operator==(const Iterator& other) const noexcept = default;

    private:
        friend class Keystore;

        const Keystore* keystore_ = nullptr;
        KeystoreIndex type_ = KeystoreIndex::Prefix;
        size_t index_ = 0;

        Iterator(const Keystore* keystore, KeystoreIndex type, size_t index) noexcept;
    };

    static_assert(std::forward_iterator<Keystore::Iterator>);
}
